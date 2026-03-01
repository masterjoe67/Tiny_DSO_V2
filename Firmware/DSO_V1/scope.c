#include <stdint.h>
#include <stdlib.h>
#include <util/delay.h>
#include <stdbool.h>
#include <math.h>
#include "Peripheral/st7798.h"
#include "Peripheral/input.h"
#include "Peripheral/uart.h"
#include "scope.h"






//int16_t old_buffer_a[400];
//int16_t old_buffer_b[400];

// Definiamo l'inizio della RAM "extra" (dopo i primi 4KB dell'ATmega128)
// L'indirizzo 0x1100 è l'inizio della zona oltre i 4KB standard
#define RAM_EXTRA_START 0x1100
#define ADDR_OLD_A (int16_t*)(0x1740)
#define ADDR_OLD_B (int16_t*)(0x1A60)

// Creiamo dei puntatori che puntano a quella zona
uint16_t *ch1_buffer = (uint16_t *)(RAM_EXTRA_START);
uint16_t *ch2_buffer = (uint16_t *)(RAM_EXTRA_START + 800); // 800 byte dopo (400 samples * 2)

// Puntatori ai buffer "storici" (vecchi dati per cancellazione)
int16_t *old_buffer_a = (int16_t *)(RAM_EXTRA_START + 1600);
int16_t *old_buffer_b = (int16_t *)(RAM_EXTRA_START + 2400);

// Mappiamo la struct dopo i buffer dei campioni (es. a 0x3000)
ScopeMeasures *misure = (ScopeMeasures *)0x3000;

Channel ch1, ch2;

uint8_t time_div_sel = 10;
uint8_t prev_time_div_sel = 0xFF; // valore precedente (inesistente all'inizio)
uint16_t prev_trigger_level = 0xFFFF; // valore precedente (inesistente all'inizio)
int16_t view_offset = 0;
int16_t prev_view_offset = 0xFFFF;
int16_t prev_det_sig = 0;
bool freeze = false;
bool pan_flag = false;
bool time_div_sel_changed = true;

static trigger_mode_t trigger_mode = TRIG_MODE_AUTO;
static trig_slope_t trigger_slope = TRIG_SLOPE_RISING;
static uint8_t trigger_source = 1;
static uint8_t old_meas_source = 1;
static uint8_t old_meas_type = 1;
static uint8_t old_meas_active = 2;
static uint8_t old_f_active = 0;

uint8_t currentMenu = MENU_CH1; // Default

static Point_t old_trig_a, old_trig_b, old_trig_c;
static int16_t last_trig_y = -100; // Inizializzato fuori schermo

int16_t* buffers_vecchi[2] = { ADDR_OLD_A, ADDR_OLD_B };
uint16_t trigger_level_12bit = 0x07FF;

static int16_t last_enc1 = 0;
static int16_t last_enc2 = 0;


uint8_t current_time_base_idx = 0;
static bool is_running= true;

const float v_div_values[] = {
    0.01, 0.02, 0.05,   // 10mV, 20mV, 50mV
    0.1,  0.2,  0.5,    // 100mV, 200mV, 500mV
    1.0,  2.0,  5.0,    10.0 // 1V, 2V, 5V, 10V
};

const char* v_div_labels[] = {
    "10mV", "20mV", "50mV", 
    "100mV", "200mV", "500mV", 
    "1V", "2V", "5V","10V"
};


uint8_t old_current_time_base_idx = 0xFF;
uint16_t old_trigger_level_12bit = 0xFFFF;
uint32_t old_freq = 0xFFFFFFFF;

Point_t old_a = { 0, 0 };
Point_t old_b = { 0, 0 };
Point_t old_c = { 0, 0 };
Point_t gnd_mark_a[2] = {{ 0, 0 }, { 0, 0 }};
Point_t gnd_mark_b[2] = {{ 0, 0 }, { 0, 0 }};
Point_t gnd_mark_c[2] = {{ 0, 0 }, { 0, 0 }};


const char* time_base_labels[] = {
    "1us",   "2us",   "5us",   "10us",  "20us",  "50us", 
    "100us", "200us", "500us", "1ms",   "2ms",   "5ms", 
    "10ms",  "20ms",  "50ms",  "100ms", "200ms", "500ms", 
    "1s"
};


//Prototipe
void draw_trigger_line(uint16_t level12, uint16_t color, bool erase);
float calcolaVoltReali(Channel *ch, uint8_t valoreADC_8bit);
//int16_t calcolaYTraccia(Channel *ch, uint8_t valoreADC);
int16_t calcolaYTraccia(Channel *ch, uint16_t valoreADC_16bit);

// Algoritmo Integer Square Root (veloce per AVR)
uint32_t isqrt(uint32_t n) {
    uint32_t res = 0;
    uint32_t bit = 1UL << 30; // Il bit più alto possibile

    // "bit" parte dalla potenza di 4 più alta <= n
    while (bit > n) bit >>= 2;

    while (bit != 0) {
        if (n >= res + bit) {
            n -= res + bit;
            res = (res >> 1) + bit;
        } else {
            res >>= 1;
        }
        bit >>= 2;
    }
    return res;
}

void calculate_measures(int16_t *buffer, uint16_t size) {
    uint32_t sum_sq = 0;   // Per il calcolo RMS (v^2)
    int32_t  sum_raw = 0;  // Per il valore medio
    int16_t  max_v = -2048; // Assumendo dati bipolari o centrati
    int16_t  min_v =  2047;

    for (uint16_t i = 0; i < size; i++) {
        int16_t val = buffer[i];

        // 1. Ricerca Min/Max per Vpp
        if (val > max_v) max_v = val;
        if (val < min_v) min_v = val;

        // 2. Accumulo per Media
        sum_raw += val;

        // 3. Accumulo quadrati per RMS
        // Usiamo i 32 bit perché 2048^2 = 4.194.304 (ci sta largo)
        sum_sq += (uint32_t)((int32_t)val * val);
    }

    // Risultati finali scritti direttamente in XRAM
    misure->vpp  = max_v - min_v;
    misure->vavg = sum_raw / size;
    
    // RMS = Radice quadrata della media dei quadrati
    // La funzione sqrt() di avr-libc è ottimizzata
    misure->vrms = (uint16_t)sqrt((double)sum_sq / size);
}

void set_base_time(uint8_t index) {
    // Limite di sicurezza a 1s (indice 18)
    if (index > MAX_TIMEBASE_IDX) index = MAX_TIMEBASE_IDX;
    
    current_time_base_idx = index;

    // Scrittura nel registro MMIO dell'FPGA
    REG_BASETIME = index;

}

void set_trigger_level(uint16_t level12)
{
    level12 &= 0x0FFF; 
    uint8_t b0 = (uint8_t)(level12 & 0xFF);         // Primi 8 bit (0-7)
    uint8_t b1 = (uint8_t)((level12 >> 8) & 0x0F);  // Altri 4 bit (8-11)

    REG_TRIGGER_LEVEL = b0;   // bytecnt 00
    REG_TRIGGER_LEVEL = b1;   // bytecnt 01
    REG_TRIGGER_LEVEL = 0x00; // bytecnt 10 -> triggera il latch del valore
}

void set_trigger_mode(trigger_mode_t mode, trig_slope_t slope, uint8_t source)
{
    source -= 1;
    if(source > 1) source = 1;
    uint8_t v = 0;

    v |= (mode & 0x3) << 6;       // bits 7..6 = mode
    v |= (source & 0x3) << 4;     // bits 5..4 = source (00=CH1, 01=CH2)
    v |= (slope & 1) << 3;        // bit 3 = edge
    v |= (1 << 2);                // trig_enable = 1
    v |= (0 << 0);                // rearm = 0

    TRIG_CTRL_REG = v;
    trigger_mode = mode;
    trigger_slope = slope;

}

/***************************************************************************************
** Function name:           draw_trace
** Description:             Disegna e cancella la traccia usando la logica V/div
***************************************************************************************/
void draw_dual_trace_from_bram(Channel *ch_a, Channel *ch_b, int16_t *old_buf_a, int16_t *old_buf_b, uint16_t length, bool vectors)
{
    const int16_t Y_MIN = MARGIN_Y;
    const int16_t Y_MAX = MARGIN_Y + TRACE_H;

    // Variabili per il calcolo misure (locali per massimizzare la velocità nei registri)
    uint32_t sum_sq = 0;   
    int32_t  sum_raw = 0;  
    int16_t  max_v = -32768; // Inizializzati al minimo/massimo possibile per int16
    int16_t  min_v =  32767;
    
    int16_t y_prev_new_a = -100, y_prev_old_a = -100; 
    int16_t y_prev_new_b = -100, y_prev_old_b = -100; 

   

    for (uint16_t i = 0; i < length; i++) {
        uint16_t x = i + MARGIN_X;

        // --- GESTIONE CANALE A ---
        if (old_buf_a[i] > Y_MIN && old_buf_a[i] < Y_MAX) {
            if (vectors && i > 0 && y_prev_old_a > Y_MIN) tft_drawLine_Clipped(x-1, y_prev_old_a, x, old_buf_a[i], BLACK, Y_MIN, Y_MAX);
            else tft_drawPixel(x, old_buf_a[i], BLACK);
        }
        y_prev_old_a = old_buf_a[i];

        if (ch_a->enabled) {
            int16_t y_now_a = calcolaYTraccia(ch_a, ch1_buffer[i]);
            if (y_now_a > Y_MIN && y_now_a < Y_MAX) {
                if (vectors && i > 0 && y_prev_new_a > Y_MIN) tft_drawLine_Clipped(x-1, y_prev_new_a, x, y_now_a, ch_a->color, Y_MIN, Y_MAX);
                else tft_drawPixel(x, y_now_a, ch_a->color);
            }
            y_prev_new_a = y_now_a;
            old_buf_a[i] = y_now_a;
        } else { old_buf_a[i] = -100; }

        // --- GESTIONE CANALE B ---
        if (old_buf_b[i] > Y_MIN && old_buf_b[i] < Y_MAX) {
            if (vectors && i > 0 && y_prev_old_b > Y_MIN) tft_drawLine_Clipped(x-1, y_prev_old_b, x, old_buf_b[i], BLACK, Y_MIN, Y_MAX);
            else tft_drawPixel(x, old_buf_b[i], BLACK);
        }
        y_prev_old_b = old_buf_b[i];

        if (ch_b->enabled) {
            int16_t y_now_b = calcolaYTraccia(ch_b, ch2_buffer[i]);
            if (y_now_b > Y_MIN && y_now_b < Y_MAX) {
                if (vectors && i > 0 && y_prev_new_b > Y_MIN) tft_drawLine_Clipped(x-1, y_prev_new_b, x, y_now_b, ch_b->color, Y_MIN, Y_MAX);
                else tft_drawPixel(x, y_now_b, ch_b->color);
            }
            y_prev_new_b = y_now_b;
            old_buf_b[i] = y_now_b;
        } else { old_buf_b[i] = -100; }

        // --- ACCUMULO DATI PER MISURE (Ottimizzato) ---
        if(misure->active) {
            // Seleziona il campione grezzo in base alla sorgente del menu
            // Nota: sorgente 1 = CH1, sorgente 2 = CH2 (adattalo se i tuoi indici sono 0 e 1)
            int16_t val = (misure->source == 1) ? ch1_buffer[i] : ch2_buffer[i];

            if (val > max_v) max_v = val;
            if (val < min_v) min_v = val;
            sum_raw += val;
            sum_sq += (uint32_t)((int32_t)val * val);
        }
    } 

    // --- CALCOLO FINALE E CONVERSIONE IN VOLT ---

    // --- CALCOLO FINALE CON PRECISIONE FLOAT ---
    if(misure->active) {
    // 1. Identifica il canale (Sorgente 1 = A, 2 = B)
    Channel *ch_src = (misure->source == 1) ? ch_a : ch_b;

    // 1. COSTANTE FISSA DI CALIBRAZIONE (Quanto vale 1 bit dell'ADC in Volt)
    // Esempio: se il tuo ingresso accetta 3.3V max, metti 3.3f
    // Se hai un partitore fisso che porta 20V a 3.3V, metti 20.0f
    const float VOLT_FONDO_SCALA = 3.3f; 
    const float LSB_BASE = VOLT_FONDO_SCALA / 4096.0f;

    // 2. APPLICAZIONE DEL MOLTIPLICATORE (Attenuatore Sonda)
    // Questo è l'unico parametro del menu che influenza la misura reale!
    float lsb_reale = LSB_BASE * ch_src->multiplier;

    // 3. Vpp: Forza la sottrazione a float PRIMA della moltiplicazione
    misure->vpp = (float)(max_v - min_v) * lsb_reale;

    // 4. Vavg: Questa è la riga più critica! 
    // Se sum_raw e length sono interi, DEVI castarli entrambi.
    misure->vavg = ((float)sum_raw / (float)length) * lsb_reale;
    
    // 5. Vrms: Usa sqrtf (specifica per float) invece di sqrt (per double)
    float mean_sq = (float)sum_sq / (float)length;
    misure->vrms = sqrtf(mean_sq) * lsb_reale;
}
}

void osc_wait_ready(void)
{
    // Bit READY già implementato in lettura da REG_TRIG o bit dedicato
    while (!(REG_TRIG & (1 << READY_BIT))) {
        // attesa attiva finché READY non diventa 1
        
    }
}

void rearm(){
    if (trigger_mode == TRIG_MODE_SINGLE && freeze){
        REG_TRIG = 0x01;
        freeze = false;
    }
}

static inline void osc_arm_readout(void)
{
    REG_INDEX = 0;
}

void tft_drawGrid(uint16_t color) {
    int16_t xStart = MARGIN_X;
    int16_t yStart = MARGIN_Y;
    int16_t xEnd   = MARGIN_X + TRACE_W;
    int16_t yEnd   = MARGIN_Y + TRACE_H;

    uint8_t gridSpacing = 40;  // Orizzontale (Tempo)
    uint8_t gridVSpacing = 30; // Verticale (Tensione)
    uint8_t dotSpacing  = 4;

    // Calcoliamo le coordinate centrali
    // Nota: Assicurati che TRACE_W/2 e TRACE_H/2 siano multipli di gridSpacing
    int16_t xCenter = xStart + (TRACE_W / 2);
    int16_t yCenter = yStart + (TRACE_H / 2);

    // 1. Linee Orizzontali
    for (int16_t y = yStart; y <= yEnd; y += gridVSpacing) {
        // Se è la linea centrale orizzontale, usiamo passo 1 (linea continua)
        // altrimenti usiamo dotSpacing
        uint8_t step = (y == yCenter) ? 2 : dotSpacing;
        
        for (int16_t x = xStart; x <= xEnd; x += step) {
            tft_drawPixel(x, y, color);
        }
    }

    // 2. Linee Verticali
    for (int16_t x = xStart; x <= xEnd; x += gridSpacing) {
        // Se è la linea centrale verticale, usiamo passo 1 (linea continua)
        // altrimenti usiamo dotSpacing
        uint8_t step = (x == xCenter) ? 2 : dotSpacing;

        for (int16_t y = yStart; y <= yEnd; y += step) {
            tft_drawPixel(x, y, color);
        }
    }
}

void osc_read_triggered()
{
    /* 1. Controllo preliminare: se siamo in freeze, non aggiorniamo nulla */
    if (freeze) {
        // Opzionale: se serve rinfrescare il readout hardware
        // osc_arm_readout(); 
        return; 
    }

    /* 2. Controllo Trigger NON BLOCCANTE */
    // Verifichiamo se l'FPGA è READY. Se non lo è, usciamo immediatamente.
    // In questo modo i buffer 'a' e 'b' restano intatti con i vecchi dati.
    if (!(REG_TRIG & (1 << READY_BIT))) {
        return; // Torna al main: i tasti e il Pan risponderanno subito!
    }

    /* 3. Se arriviamo qui, l'FPGA è scattata (Dati pronti!) */
    if (trigger_mode == TRIG_MODE_SINGLE) {
        freeze = true; // Blocca i futuri aggiornamenti
    }
    
    // Avvia trasferimento dati da FPGA a AVR
    osc_arm_readout(); 

    // 4. LETTURA DALLA BRAM (Sempre 4 byte per sincronismo FPGA)
        // Offset 0,1 -> Canale A | Offset 2,3 -> Canale B
        // --- RESET INDICE HARDWARE ---
    INDEX_RESET = 0x01; 
    for(uint16_t i = 0; i < 400; i++) {
        uint8_t a_l = BRAM_DATA_PTR[0];
        uint8_t a_h = BRAM_DATA_PTR[1];
        uint8_t b_l = BRAM_DATA_PTR[2];
        uint8_t b_h = BRAM_DATA_PTR[3]; // Qui l'FPGA incrementa reg_index_int
        ch1_buffer[i] = (a_h << 8) | a_l;
        ch2_buffer[i] = (b_h << 8) | b_l;
    }
    /* 5. Riarmo Trigger automatico */
    if (is_running && !(trigger_mode == TRIG_MODE_SINGLE && freeze)) {
        REG_TRIG = 0x01;
    }
}

static inline void osc_write_view_offset(int16_t offset)
{
    REG_BASETIME = 0xFF;                         // escape
    REG_BASETIME = 0x01;                         // comando: view_offset
    REG_BASETIME = (uint8_t)(offset & 0xFF);     // LSB
    REG_BASETIME = (uint8_t)(offset >> 8);       // MSB
    REG_BASETIME = 0xFF;

}


void drawPanTrack(){

    int16_t offset = (TRACE_W / 2) - view_offset + MARGIN_X + 1; // +1 per allineare meglio il triangolo alla griglia

    Point_t a = { offset - 5, MARGIN_Y };
    Point_t b = { offset + 5, MARGIN_Y };
    Point_t c = { offset, MARGIN_Y + 10 };
    
    if(pan_flag){
        tft_FillTriangle(old_a, old_b, old_c, BLACK);
        old_a = a;
        old_b = b;
        old_c = c;
    }
    tft_FillTriangle(a, b, c, WHITE);
}

/***************************************************************************************
** Function name:           calcolaYTraccia
** Description:             Converte il valore ADC in coordinata Y pixel basandosi
** sui parametri del canale (V/div, offset, invert).
***************************************************************************************/
int16_t calcolaYTraccia(Channel *ch, uint16_t valoreADC_12bit) {
    // --- COSTANTI PER ADC A 12 BIT ---
    const float ADC_MAX = 4095.0f;
    const float ADC_ZERO = 2048.0f; // Metà scala per accoppiamento AC/Dual
    const float PIXEL_PER_DIV = 30.0f;

    // 1. Calcoliamo lo spostamento grezzo rispetto allo zero dell'ADC
    // (Il valore può essere positivo o negativo)
    float deltaADC = (float)valoreADC_12bit - ADC_ZERO;

    // 2. Calcoliamo il fattore di scala totale
    // Questo trasforma il valore ADC direttamente in "pixel di spostamento"
    // Formula: (Delta / 4095 * 3.3V * Multiplier / VoltsDiv) * 30 pixel
    float fattoreSpostamento = (deltaADC * 3.3f / ADC_MAX); // * ch->multiplier;
    float divisioni = fattoreSpostamento / ch->volts_div;
    
    if (ch->inverted) divisioni = -divisioni;

    // 3. Calcolo pixel finali
    int16_t pixelSpostamento = (int16_t)(divisioni * PIXEL_PER_DIV);

    // 4. POSIZIONE SULLO SCHERMO
    // ch->offset è la posizione della linea dello zero scelta dall'utente sul TFT
    return ch->offset - pixelSpostamento;
}

void draw_ground_marker(Channel *ch) {
    // 1. La posizione Y dello zero è data direttamente dal valore di ch->offset
    int16_t y_zero = ch->offset;

    // 2. Gestione della cancellazione
    // Se l'offset è cambiato, cancelliamo il triangolo nella vecchia posizione
    if (ch->offset != ch->old_offset) {
        // Calcoliamo i vecchi vertici basandoci su old_offset per cancellarli con precisione
        Point_t old_a, old_b, old_c;
        uint16_t x_tip = MARGIN_X + 8;
        uint16_t x_base = MARGIN_X;

        old_a.x = x_base; old_a.y = ch->old_offset - 5;
        old_b.x = x_base; old_b.y = ch->old_offset + 5;
        old_c.x = x_tip;  old_c.y = ch->old_offset;

        tft_FillTriangle(old_a, old_b, old_c, BLACK);
        
        // Aggiorniamo old_offset dopo la cancellazione
        ch->old_offset = ch->offset;
    }

    // 3. Calcolo nuove coordinate del marker
    uint16_t x_tip = MARGIN_X + 8;
    uint16_t x_base = MARGIN_X;

    ch->gnd_mark_a.x = x_base;
    ch->gnd_mark_a.y = y_zero - 5;
    
    ch->gnd_mark_b.x = x_base;
    ch->gnd_mark_b.y = y_zero + 5;
    
    ch->gnd_mark_c.x = x_tip;
    ch->gnd_mark_c.y = y_zero;

    // 4. Scelta del colore (pieno se focused, spento se no)
    uint16_t draw_color = ch->color;
    if (!ch->focused) {
        // Scuriamo il colore per i canali non selezionati (effetto Tek)
        draw_color = (ch->color >> 1) & 0x7BEF; 
    }

    // 5. Disegno del triangolo nelle nuove coordinate
    if (ch->enabled) {
        tft_FillTriangle(ch->gnd_mark_a, ch->gnd_mark_b, ch->gnd_mark_c, draw_color);
        
        // Se vuoi aggiungere il numero del canale (opzionale)
        // tft_setTextColor(WHITE);
        // tft_printAt(ch == &ch1 ? "1" : "2", x_base + 1, y_zero - 4, WHITE, BLACK, 1);
    }
}

void acquire_and_draw(){
    // 1. ACQUISIZIONE (Condizionale)
    // Proviamo a leggere solo se siamo in RUN o in un SINGLE attivo
    //if (is_running || (trigger_mode == TRIG_MODE_SINGLE && !freeze)) {
    if (is_running || (trigger_mode == TRIG_MODE_SINGLE && !freeze) || pan_flag) {
        osc_read_triggered();
    }


    
    tft_drawGrid(LIGHTGREY);

    draw_dual_trace_from_bram(&ch1, &ch2, old_buffer_a, old_buffer_b, 400, true);

    
    // UI e Marker (Sempre visibili per poterli muovere in STOP)
    draw_trigger_line(trigger_level_12bit, ORANGE, false);
    draw_ground_marker(&ch1);
    draw_ground_marker(&ch2);
    drawPanTrack();
}

void drawMenuButton(uint8_t index, const char* label, const char* data, bool active, uint16_t color) {
    uint16_t y = 25 + (index * 50); // Calcola posizione Y in base all'indice
    uint16_t bgColor = BLACK;       // Definiamo lo sfondo fisso a nero
    
    tft_fillRect(410, y, 65, 40, bgColor); // Pulisce l'area del bottone prima di ridisegnarlo
    // 1. Disegna la cornice del bottone
    tft_drawRect(410, y, 65, 40, color);
    

    // 3. Scrivi il testo passando tutti i parametri richiesti dalla tua funzione
    tft_printCenteredX(label, 410, 475, y + 5, color, bgColor, 1); 
    tft_printCenteredX(data, 410, 475, y + 20, color, bgColor, 2);
}

void drawStaticInterface() {
    // 1. Pulisce tutto lo schermo
    tft_fillScreen(BLACK);
    
    // 2. Barra Superiore (Status e Misure rapide)
    tft_fillRect(0, 0, 480, 20, DARKGREY);
    tft_printAt("Mje", 10, 5, GREEN, DARKGREY, 2);
    //tft_printAt("T: 100uS", 120, 5, WHITE, DARKGREY);
    //tft_printAt("Vpp: 3.24V", 250, 5, YELLOW, DARKGREY);

    // --- TITOLO MENU A DESTRA (Sopra i tasti) ---
    const char* menuName;
    if (currentMenu == MENU_CH1)      menuName = "CH 1";
    else if (currentMenu == MENU_CH2) menuName = "CH 2";
    else if (currentMenu == MENU_TRIG) menuName = "TRIG";
    else                              menuName = "MENU";
    
    tft_printAt(menuName, 430, 5, WHITE, DARKGREY, 2);

    // 3. Cornice Area Traccia (400x240)
    tft_drawRect(MARGIN_X - 1, MARGIN_Y - 1, TRACE_W + 2, TRACE_H + 2, WHITE);
    
    // 4. Linea di divisione Sidebar
    tft_drawLine(SIDEBAR_X - 2, 20, SIDEBAR_X - 2, TRACE_H + MARGIN_Y, GREY);

    updateSidebarLabels(); // Aggiorna tutte le etichette in base allo stato attuale (usa i dati nelle struct)
    // 6. Ripristina la griglia
    tft_drawGrid(LIGHTGREY);
}


void cycleCoupling(Channel *ch) 
{
    // 1. Cicla tra 0, 1 e 2 direttamente nella struct del canale
    // Usiamo le tue costanti (COUPL_DC, COUPL_AC, COUPL_GND)
    ch->coupling++;
    if (ch->coupling > COUPL_GND) {
        ch->coupling = COUPL_DC;
    }

    // 2. Comunicazione Hardware (VHDL/FPGA)
    // Se serve l'indice 1 o 2 per l'hardware:
    // uint8_t chNum = (ch == &ch1) ? 1 : 2;
    // inviaComandoHardware(chNum, ch->coupling);

    // 3. Preparazione etichetta
    const char* data;
    switch(ch->coupling) {
        case COUPL_DC:  data = "DC";  break;
        case COUPL_AC:  data = "AC";  break;
        case COUPL_GND: data = "GND";  break;
        default:        data = "??";   break;
    }
    
    // 4. Feedback visivo
    // Usiamo il colore contenuto nella struct (YELLOW o CYAN/BLUE)
    // per far capire subito all'utente su quale canale sta agendo.
    //uint16_t color = ch->color; 

    // Ridisegna il bottone (indice 1 della sidebar)
    // Passiamo la label, true per indicare che è "attivo" e il colore del canale
    drawMenuButton(0, "Coupling", data, true, WHITE); 
}

void toggleBWLimit(Channel *ch) 
{
    // 1. Inverte lo stato del filtro (0 o 1)
    ch->bw_limit = !ch->bw_limit;

    // 2. Comunicazione Hardware (Fondamentale!)
    // Il BW Limit nei veri oscilloscopi attiva un filtro passa-basso analogico o digitale.
    // Se la tua FPGA o il tuo front-end lo supportano:
    // uint8_t chNum = (ch == &ch1) ? 1 : 2;
    // set_fpga_bw_filter(chNum, ch->bw_limit);

    // 3. Preparazione etichetta per il menu
    // "Full" significa banda passante massima, "20M" è il limite standard
    const char* data = ch->bw_limit ? "20M" : "FULL";
    
    // 4. Feedback visivo
    // Usiamo il colore del canale per evidenziare quando il filtro è attivo
    //uint16_t color = ch->bw_limit ? ch->color : WHITE;

    // Supponiamo di usare il tasto 4 della sidebar per il BW Limit
    drawMenuButton(1, "BW Limit", data, true, WHITE); 
}


void aggiornaMoltiplicatoreSonda(Channel *ch) 
{
    // Usiamo direttamente ch->probe (che è già stato aggiornato da cycleProbe)
    // per impostare il moltiplicatore interno alla struct
    switch(ch->probe) {
        case 0: // 1X
            ch->multiplier = 1.0f;
            break;
        case 1: // 10X
            ch->multiplier = 10.0f;
            break;
        case 2: // 100X
            ch->multiplier = 100.0f;
            break;
        default:
            ch->multiplier = 1.0f;
            break;
    }
}

void cycleProbe(Channel *ch) 
{
    // 1. Cicla tra le tre impostazioni direttamente nella struct (0=1X, 1=10X, 2=100X)
    ch->probe++;
    if (ch->probe > 2) {
        ch->probe = 0;
    }

    // 2. Logica di calcolo
    // Passiamo il puntatore così la funzione sa già tutto quello che serve
    aggiornaMoltiplicatoreSonda(ch);

    // 3. Preparazione etichetta
    const char* data;
    switch(ch->probe) {
        case 0:  data = "1X"; break;
        case 1:  data = "10X"; break;
        case 2:  data = "100X"; break;
        default: data = "??  "; break;
    }
    
    // 4. Aggiornamento grafico con il colore del canale
    // Usiamo ch->color così se sei nel menu CH1 è Giallo, se CH2 è Ciano
    //uint16_t color = ch->color;

    // Disegniamo il bottone (indice 2 della sidebar per il tasto Probe)
    drawMenuButton(3, "Probe", data, true, WHITE); 
}

float calcolaVoltReali(Channel *ch, uint8_t valoreADC_8bit) {
    // 1. Convertiamo il valore 0-255 in tensione (assumendo range 3.3V)
    // Usiamo 255.0f per forzare il calcolo in virgola mobile
    float tensioneIngresso = ((float)valoreADC_8bit * 3.3f) / 255.0f;
    
    // 2. Moltiplichiamo per il fattore della sonda (ch->multiplier)
    // Se la sonda è 10x, il valore reale è tensioneIngresso * 10
    return (float)tensioneIngresso * (float)ch->multiplier;
}


void updateSidebarLabels() {
    // --- 1. AGGIORNAMENTO NOME MENU NELLA BARRA SUPERIORE ---
    const char* menuTitle;
    uint16_t menuColor; // Variabile per il colore del titolo
    switch (currentMenu) {
        case MENU_CH1:
            menuTitle = "CH 1";
            menuColor = ch1.color;  // Colore traccia 1
            break;
            
        case MENU_CH2:
            menuTitle = "CH 2";
            menuColor = ch2.color;    // Colore traccia 2
            break;
            
        case MENU_TRIG:
            menuTitle = "TRIGER";
            menuColor = YELLOW; // Colore linea trigger
            break;
            
        case MENU_MEAS:
            menuTitle = "MEASURE";
            menuColor = WHITE;
            break;
        
        /*case MENU_PAN:
            menuTitle = " PAN  ";
            menuColor = MAGENTA;
            break;*/
            
        default:
            menuTitle = " MENU ";
            menuColor = CYAN;
            break;
    }
    
    // Scriviamo il titolo del menu centrato sopra i tasti, con il colore specifico
    tft_fillRect(410, 0, 480, 20, DARKGREY);
    tft_printCenteredX(menuTitle, 410, 475, 5, menuColor, DARKGREY, 2); // Opzione centrata

    // --- 2. LOGICA TASTI SIDEBAR ---

    if (currentMenu == MENU_CH1 || currentMenu == MENU_CH2) {
        // 1. Puntatore al canale basato sul menu aperto
        Channel *ch = (currentMenu == MENU_CH1) ? &ch1 : &ch2;
        //const char* chName = (currentMenu == MENU_CH1) ? "CH1" : "CH2";

        // TASTO 0: Accoppiamento (Aggiungi 'coupling' alla struct!)
        // 0: DC, 1: AC, 2: GND
        const char* couplLabels[] = {"DC", "AC", "GND"};
        drawMenuButton(0, "Coupling", couplLabels[ch->coupling], true, WHITE);

        // TASTO 1: Limite banda (Non implementato, mettiamo un placeholder)
        drawMenuButton(1, "BW Limit", "FULL ",false, WHITE);

        // TASTO 2: Volt/div (Focus sull'encoder, ma mostriamo anche il moltiplicatore della sonda)
        //char vdivLabel[10];
        //sprintf(vdivLabel, "V/DIV: %.1f", ch->volts_div);
        const char* vdivLabel[] = {"COARSE", "Fine"};
        drawMenuButton(2, "V/DIV", vdivLabel[ch->isFine], false, WHITE);

        // TASTO 3: Sonda (Aggiungi 'probe' alla struct!)
        const char* probeLabels[] = {"1X", "10X", "100X"};
        drawMenuButton(3, "Probe", probeLabels[ch->probe], true, WHITE);

        // TASTO 3: Inversione (Usiamo ch->inverted)
        drawMenuButton(4, "Invert", ch->inverted ? "ON" : "OFF", ch->inverted, WHITE);

    }
    
    else if (currentMenu == MENU_TRIG) {
        // TASTO 0: Modalità (AUTO/NORMAL) - Usiamo i nuovi nomi
        //(0, (trigger_mode == 0) ? "AUTO  " : "NORM  ", true, WHITE);
        drawMenuButton(0, "Source", (trigger_source == 1) ? "CH1" : "CH2", true, WHITE);
        // TASTO 1: Slope (RISE/FALL) - Usiamo i nuovi nomi
        //(1, (trigger_slope == 1) ? "RISE" : "FALL", true, WHITE);
        drawMenuButton(1, "Slope", trigger_slope ? "RISE" : "FALL", true, WHITE);
        // TASTO 2: Sorgente (CH1/CH2) - Usiamo i nuovi nomi
        //drawMenuButton(2, (trigger_source == 1) ? "SRC: CH1" : "SRC: CH2", true, WHITE);
        drawMenuButton(2, "Mode", (trigger_mode == 0) ? "AUTO" : "NORM", true, WHITE);
        // TASTO 3: 
        drawMenuButton(3, "", "", true, WHITE);
        drawMenuButton(4, "", "", true, WHITE);
        
    }
    else if (currentMenu == MENU_MEAS) {
        drawMenuButton(0, "Source", (misure->source == 1) ? "CH1" : "CH2", true, WHITE);

        const char* measLabels[] = {"VPP", "VAVG", "VRMS"};
        drawMenuButton(1, "Type", measLabels[misure->type], true, WHITE);


        drawMenuButton(2, "Show", (misure->active == 0) ? "OFF" : "ON", true, WHITE);
        drawMenuButton(3, "Freq.", (misure->f_active == 0) ? "OFF" : "ON", true, WHITE);
        drawMenuButton(4, "", "", true, WHITE);
        
    }

    /*else if (currentMenu == MENU_TBASE) {
        
        drawMenuButton(0, "       ", true, WHITE);
        drawMenuButton(1, "       ", true, WHITE);
        drawMenuButton(2, "       ", true, WHITE);
        drawMenuButton(3, "       ", true, WHITE);
        drawMenuButton(4, "       ", true, WHITE);
  
    }
    else if (currentMenu == MENU_PAN) {
        
        drawMenuButton(0, "       ", true, WHITE);
        drawMenuButton(1, "       ", true, WHITE);
        drawMenuButton(2, "       ", true, WHITE);
        drawMenuButton(3, "       ", true, WHITE);
        drawMenuButton(4, "       ", true, WHITE);
  
    }*/
}

void toggleFineCoarse(Channel *ch, int16_t *last_enc) {
    ch->isFine = !ch->isFine;

    // Determiniamo quale encoder usare: 1 per CH1, 3 per CH2
    // Assumendo che ch1 e ch2 siano le tue istanze globali
    uint8_t enc_id = (ch == &ch1) ? 1 : 3;

    if (ch->isFine) {
        // --- DA COARSE A FINE ---
        int16_t current_units = (int16_t)(ch->volts_div * 100.0f);

        uart_print("CH ");
        uart_print_int16(enc_id);
        uart_print(" - Fine Mode - Units: ");
        uart_print_int16(current_units);
        uart_print("\r\n");

        configure_encoder(enc_id, PARAM_MIN, 1);    // 0.01V
        configure_encoder(enc_id, PARAM_MAX, 1000); // 10.0V
        configure_encoder(enc_id, PARAM_STEP, 1);
        configure_encoder(enc_id, PARAM_C_VAL, current_units);
        
        *last_enc = current_units; 
    }
    else {
        // --- RITORNO A COARSE ---
        float min_diff = 100.0f;
        for (uint8_t i = 0; i < 10; i++) {
            float diff = fabs(ch->volts_div - v_div_values[i]);
            if (diff < min_diff) {
                min_diff = diff;
                ch->vdiv_idx = i;
            }
        }
        ch->volts_div = v_div_values[ch->vdiv_idx];

        configure_encoder(enc_id, PARAM_MIN, VDIVCH_MIN);
        configure_encoder(enc_id, PARAM_MAX, VDIVCH_MAX);
        configure_encoder(enc_id, PARAM_STEP, 1);
        configure_encoder(enc_id, PARAM_C_VAL, ch->vdiv_idx);
        
        *last_enc = ch->vdiv_idx;
    }

    // Aggiorna interfaccia (usando l'indice corretto del tasto, qui 2)
    drawMenuButton(2, "V/DIV", ch->isFine ? "FINE" : "COARSE", ch->isFine, WHITE);
}

void toggleInvert(Channel *ch) 
{
    // 1. Inverte lo stato booleano direttamente nella struct
    ch->inverted = !ch->inverted;

    // 2. Comunicazione Hardware (Opzionale)
    // Se la tua FPGA ha un registro per l'inversione hardware:
    // set_fpga_inversion(ch == &ch1 ? 0 : 1, ch->inverted);

    // 3. Preparazione etichetta
    const char* label = ch->inverted ? "ON" : "OFF";
    
    // 4. Feedback visivo
    // Usiamo il colore del canale per il tasto se è attivo, 
    // così l'utente ha un feedback immediato (Stile Tektronix)
    //uint16_t buttonColor = ch->inverted ? ch->color : WHITE;

    // Disegniamo il pulsante (Tasto 3 nel menu)
    drawMenuButton(4, "Invert", label, ch->inverted, WHITE); 

}

float read_fpga_frequency() {
    uint32_t period = 0;
    uint8_t v0, v1, v2, v3 = 0;

    // Leggiamo i 4 byte in sequenza dal registro REG_FREQ
    // L'FPGA incrementerà internamente l'indice del byte

    v0 = REG_FREQ0;
    v1 = REG_FREQ1;
    v2 = REG_FREQ2;
    v3 = REG_FREQ3;

    period = ((uint32_t)v3 << 24) | 
                         ((uint32_t)v2 << 16) | 
                         ((uint32_t)v1 << 8)  | 
                          (uint32_t)v0;

    
   
    //float freq = 2560000000.0f / (float)period; //40MHz
    float freq = 3840000000.0f / (float)period;
    /*uart_print("Freq ");
    uart_print_float(new_period, 1);
    uart_print("\r\n");*/
    if (period == 0) return 0;
    return freq;
    


    // Calcola la frequenza SOLO se siamo in RUN
    // o se abbiamo appena catturato un SINGLE.
    // NON calcolarla mentre muovi il Pan in STOP!
    if (is_running && !pan_flag) {
        v0 = REG_FREQ0;
        v1 = REG_FREQ1;
        v2 = REG_FREQ2;
        v3 = REG_FREQ3;

        period = ((uint32_t)v3 << 24) | 
                ((uint32_t)v2 << 16) | 
                ((uint32_t)v1 << 8)  | 
                (uint32_t)v0;

        /*if (period > 0) {
            float freq = 3840000000.0f / (float)period;
            // Aggiorna il valore a video
        }*/
    } else if (freeze) {
        // In STOP, non ricalcolare: mantieni l'ultimo valore valido a schermo
        // così la cifra non sballa mentre ti muovi nella traccia.
    }
}

void draw_trigger_line(uint16_t level12, uint16_t color, bool erase) {
    Channel *trig_ch = (trigger_source == 1) ? &ch1 : &ch2;
    int16_t y = calcolaYTraccia(trig_ch, level12); 
    
    const uint16_t RIGHT_EDGE = MARGIN_X + TRACE_W - 1;

    // --- Definizione vertici con struttura Point_t ---
    // Punta verso l'interno (sinistra)
    Point_t a = { RIGHT_EDGE,     y - 5 }; // Angolo alto sulla base
    Point_t b = { RIGHT_EDGE,     y + 5 }; // Angolo basso sulla base
    Point_t c = { RIGHT_EDGE - 7, y     }; // Punta (7 pixel verso sinistra)

    // --- 1. CANCELLAZIONE ---
    // Usiamo le vecchie coordinate memorizzate (old_a, old_b, old_c)
    if (last_trig_y != -100) {
        tft_drawFastHLine(MARGIN_X, last_trig_y, TRACE_W, BLACK);
        tft_FillTriangle(old_trig_a, old_trig_b, old_trig_c, BLACK);
    }

    if (!erase) {
        // --- 2. DISEGNO E CLIPPING ---
        if (y > MARGIN_Y && y < (MARGIN_Y + TRACE_H)) {
            
            // Disegno linea tratteggiata
            for (uint16_t x = MARGIN_X; x < RIGHT_EDGE - 8; x += 10) {
                tft_drawFastHLine(x, y, 5, color); 
            }

            // --- 3. DISEGNO TRIANGOLO ---
            tft_FillTriangle(a, b, c, color);

            // Memorizziamo per il prossimo ciclo
            old_trig_a = a;
            old_trig_b = b;
            old_trig_c = c;
            last_trig_y = y; 
        } else {
            last_trig_y = -100; 
        }
    }
}

ui_status_t get_system_status_code(void) {
    if (!is_running) {
        return UI_STATUS_STOP;
    }

    uint8_t status = REG_TRIG;
    bool fsm_ready = (status & (1 << READY_BIT));

    if (trigger_mode == TRIG_MODE_SINGLE) {
        return freeze ? UI_STATUS_STOP : UI_STATUS_WAIT;
    } 
    
    if (trigger_mode == TRIG_MODE_NORMAL) {
        return fsm_ready ? UI_STATUS_TRIGD : UI_STATUS_WAIT;
    }

    // Default per AUTO
    return UI_STATUS_RUN;
}

void draw_channel_status(Channel *ch, uint16_t xPos, uint16_t yPos, bool force) {
    // Controllo se qualcosa è cambiato o se è richiesto il refresh forzato
    if(ch->old_vdiv_idx != ch->vdiv_idx || 
       ch->old_coupling != ch->coupling || 
       ch->old_volts_div != ch->volts_div ||
       ch->old_multiplier != ch->multiplier || // Nuovo controllo per il Fine
       force) {
        ch->old_volts_div = ch->volts_div; // Aggiorniamo anche questo per il controllo Fine/Coarse
        ch->old_multiplier = ch->multiplier; // Aggiorniamo anche questo per il controllo Probe
        // Pulizia area (100px larghezza, 16px altezza)
        tft_fillRect(xPos, yPos, 120, 16, BLACK);
        
        // Colore del canale (Giallo per CH1, Ciano per CH2)
        setTextColor(ch->color, BLACK);
        tft_set_cursor(xPos, yPos);
        
        // Stampa Etichetta (CH1 o CH2 basandosi sull'indirizzo di memoria)
        tft_Print(ch == &ch1 ? "CH1: " : "CH2: ");
        
        // --- LOGICA DI STAMPA VOLTAGGIO ---

        float vdiv = ch->volts_div * ch->multiplier; // Applichiamo il moltiplicatore della sonda al valore di V/div
        
        if (vdiv < 1.0f) {
            // Sotto l'unità, meglio mostrare i mV come intero
            tft_Print_int16((int16_t)(vdiv * 1000));
            tft_Print("mV");
        } else {
            // Sopra l'unità, usiamo il float con 2 decimali
            tft_print_float(vdiv, 2); 
            tft_Print("V");
        }
        
        tft_Print(" ");
        
        // Stampa Accoppiamento
        // Usiamo un controllo semplice o l'array delle labels
        tft_Print(ch->coupling == COUPL_AC ? "AC" : 
                  ch->coupling == COUPL_GND ? "GND" : "DC");

        // Aggiorniamo i vecchi valori
        ch->old_vdiv_idx = ch->vdiv_idx;
        ch->old_coupling = ch->coupling;
    }
}

void update_status_bar(bool force) {
    uint16_t yPos = MARGIN_Y + TRACE_H + 10;
    uint16_t xStart = MARGIN_X;
    setTextSize(1);

    static ui_status_t last_ui_state = 0xFF; // Valore impossibile per forzare il primo disegno
    ui_status_t current_state = get_system_status_code();
    if (force || current_state != last_ui_state) {
        // Solo quando lo stato CAMBIA davvero, facciamo il lavoro pesante
        char* label;
        uint16_t color;

        switch (current_state) {
            case UI_STATUS_STOP:  label = "STOP  ";   color = RED;    break;
            case UI_STATUS_WAIT:  label = "WAIT  ";   color = YELLOW; break;
            case UI_STATUS_TRIGD: label = "TRIG'D"; color = GREEN;  break;
            case UI_STATUS_RUN:   label = "RUN   ";    color = GREEN;  break;
            default:              label = "???";    color = WHITE;  break;
        }

        // Qui disegni sul TFT (avviene solo una volta per ogni cambio di stato)
        // tft_draw_status(label, color); 
        tft_printAt(label, 100, 5, color, DARKGREY, 2);
        last_ui_state = current_state;
    }

    // Aggiorna info CH1
    draw_channel_status(&ch1, xStart, yPos, force);

    // Aggiorna info CH2 (spostato di 100 pixel a destra)
    draw_channel_status(&ch2, xStart + 120, yPos, force);

    

    if (misure->active) {
    // 1. Gestione BACKGROUND (solo se cambia il menu, la sorgente o se forzato)
    if (old_meas_active != misure->active || old_meas_type != misure->type || 
        old_meas_source != misure->source || force) {
        
        old_meas_active = misure->active;
        old_meas_type = misure->type;
        old_meas_source = misure->source;

        // Pulisci l'intera riga delle misure una volta sola
        tft_fillRect(xStart, yPos + 20, 240, 16, BLACK); 
    }

    // 2. AGGIORNAMENTO DATI (Sempre, finché active è true)
    switch (misure->source) {
        case 1: // CH A
            tft_set_cursor(xStart, yPos + 20);
            setTextColor(ch1.color, BLACK); 

            if (misure->type == 0) {
                tft_print_float(misure->vpp, 1); 
                tft_Print("Vpp ");
            }
            else if (misure->type == 1) {
                tft_print_float(misure->vavg, 2); 
                tft_Print("Vavg");
            }
            else if (misure->type == 2) {
                tft_print_float(misure->vrms, 2); 
                tft_Print("Vrms");
            }
            break;

        case 2: // CH B
            tft_set_cursor(xStart + 120, yPos + 20);
            setTextColor(ch2.color, BLACK); 

            if (misure->type == 0) {
                tft_print_float(misure->vpp, 2); 
                tft_Print("Vpp ");
            }
            else if (misure->type == 1) {
                tft_print_float(misure->vavg, 2); 
                tft_Print("Vavg");
            }
            else if (misure->type == 2) {
                tft_print_float(misure->vrms, 2); 
                tft_Print("Vrms");
            }
            break;
    }
} 
else {
    // 3. LOGICA DI CANCELLAZIONE ALLA DISATTIVAZIONE
    // Se lo stato è appena passato da attivo a non attivo
    if (old_meas_active != 0) {
        // Cancella l'area delle misure (sia CH A che CH B)
        tft_fillRect(xStart, yPos + 20, 240, 16, BLACK);
        
        // Aggiorna lo stato precedente così non cancella più al prossimo ciclo
        old_meas_active = 0;
    }
}
    

    // --- BASE TEMPI ---
    if(old_current_time_base_idx != current_time_base_idx || force){
        tft_fillRect(xStart + 230, yPos, 100, 16, BLACK);
        setTextColor(WHITE, BLACK);
        tft_set_cursor(xStart + 230, yPos);
        tft_Print("T: ");
        tft_Print(time_base_labels[current_time_base_idx]);
        old_current_time_base_idx = current_time_base_idx;
    tft_Print("/div");
    }

    // --- TRIGGER LEVEL ---
    if(old_trigger_level_12bit != trigger_level_12bit || force){
        tft_fillRect(xStart + 330, yPos, 100, 16, BLACK);
        setTextColor(GREEN, BLACK);
        tft_set_cursor(xStart + 330, yPos);
        tft_Print("Trig: ");
        // Calcoliamo il valore in Volt o mostriamo i bit
        // Se reg_trig_level è 0-4095 (12 bit)
        uint16_t level_mv = (uint32_t)trigger_level_12bit * 3300 / 4096; 
        tft_print_float(level_mv / 1000.0, 2);
        tft_Print("V");
        old_trigger_level_12bit = trigger_level_12bit;
    }
    // Supponiamo di aver calcolato 'freq'
    if(misure->f_active == 1) {
    float freq = read_fpga_frequency();
    
    // Aggiorniamo a video solo se la frequenza cambia o se abbiamo appena attivato la misura
    if(old_freq != freq || old_f_active == 0) {
        tft_set_cursor(MARGIN_X + 230, yPos + 20); 
        setTextColor(WHITE, BLACK);
        
        // Pulizia locale prima di scrivere il nuovo valore (opzionale se tft_Print sovrascrive bene)
        // tft_fillRect(MARGIN_X + 230, yPos + 20, 80, 10, BLACK); 

        tft_Print("F:");
        if (freq > 1000000) {
            tft_print_float(freq / 1000000.0, 2);
            tft_Print("MHz");
        } else if (freq > 1000) {
            tft_print_float(freq / 1000.0, 1);
            tft_Print("kHz");
        } else {
            tft_print_float(freq, 1);
            tft_Print("Hz "); // Spazio finale per pulire eventuali residui di cifre precedenti
        }
        
        old_freq = freq;
        old_f_active = 1; // Ricordiamo che era attivo
    }
} 
else {
    // --- LOGICA DI CANCELLAZIONE ---
    // Se prima era attivo e ora è 0, cancelliamo la scritta una volta sola
    if(old_f_active == 1) {
        // Regola le coordinate e le dimensioni in base a dove appare la scritta
        tft_fillRect(MARGIN_X + 230, yPos + 20, 90, 20, BLACK); 
        old_f_active = 0;
        old_freq = -1.0; // Reset della frequenza per forzare il refresh alla prossima riaccensione
    }
}
}

void write_encoder(uint8_t encoder_idx, int16_t value) {
    switch (encoder_idx) {
        case 0: // Encoder 0 controlla la posizione verticale di CH1
            configure_encoder(0, PARAM_MIN, OFFSET_Y_MIN);
            configure_encoder(0, PARAM_MAX, OFFSET_Y_MAX);
            configure_encoder(0, PARAM_STEP, OFFSET_Y_STEP);
            configure_encoder(0, PARAM_C_VAL, value);
            break;
        case 1: // Encoder 1 Volt/Div CH1
            configure_encoder(1, PARAM_MIN, VDIVCH_MIN);
            configure_encoder(1, PARAM_MAX, VDIVCH_MAX);
            configure_encoder(1, PARAM_STEP, VDIVCH_STEP);
            configure_encoder(1, PARAM_C_VAL, value);
            break;    
        case 2: // Encoder 2 controlla la posizione verticale di CH2
            configure_encoder(2, PARAM_MIN, OFFSET_Y_MIN);
            configure_encoder(2, PARAM_MAX, OFFSET_Y_MAX);
            configure_encoder(2, PARAM_STEP, OFFSET_Y_STEP);
            configure_encoder(2, PARAM_C_VAL, value);
            break;
        case 3: // Encoder 1 Volt/Div CH1
            configure_encoder(3, PARAM_MIN, VDIVCH_MIN);
            configure_encoder(3, PARAM_MAX, VDIVCH_MAX);
            configure_encoder(3, PARAM_STEP, VDIVCH_STEP);
            configure_encoder(3, PARAM_C_VAL, value);
            break;        
        case 4: // Encoder 4 controlla la base dei tempi
            configure_encoder(4, PARAM_MIN, TDIV_MIN);
            configure_encoder(4, PARAM_MAX, TDIV_MAX);
            configure_encoder(4, PARAM_STEP, TDIV_STEP);
            configure_encoder(4, PARAM_C_VAL, value);
            break;
        case 5: // Encoder 5 controlla il livello di trigger
            configure_encoder(5, PARAM_MIN, TRIG_MIN);
            configure_encoder(5, PARAM_MAX, TRIG_MAX);
            configure_encoder(5, PARAM_STEP, TRIG_STEP);
            configure_encoder(5, PARAM_C_VAL, value);
            break;
        case 6: // Encoder 6 controlla il Pan
            configure_encoder(6, PARAM_MIN, -PAN_LIMIT);
            configure_encoder(6, PARAM_MAX, PAN_LIMIT);
            configure_encoder(6, PARAM_STEP, PAN_STEP);
            configure_encoder(6, PARAM_C_VAL, value);
            break;
        default:
            
            break;
    }
}


void conf_encoder() {
    // Encoder 0: Posizione traccia CH1
    write_encoder(0, OFFSET_Y1_C_VAL); // Impostiamo il valore iniziale

    // Encoder 1: Volt/Div CH1
    write_encoder(1, VDIVCH_C_VAL); // Impostiamo il valore iniziale

    // Encoder 2: Posizione traccia CH2
    write_encoder(2, OFFSET_Y2_C_VAL); // Impostiamo il valore iniziale


    // Encoder 3: Volt/Div CH2
    write_encoder(3, VDIVCH_C_VAL); // Impostiamo il valore iniziale

    // Encoder 4: T/Div 
    write_encoder(4, TDIV_C_VAL); // Impostiamo il valore iniziale

    // Encoder 5: Trigger Level
    write_encoder(5, TRIG_C_VAL); // Impostiamo il valore iniziale

}


void handle_channel_button(uint8_t channel_num) {
    Channel *current = (channel_num == 1) ? &ch1 : &ch2;
    Channel *other   = (channel_num == 1) ? &ch2 : &ch1;
    uint8_t menu = (channel_num == 1) ? MENU_CH1 : MENU_CH2;
    // ch: 1 per CH1, 2 per CH2
    uint8_t idx = channel_num - 1;

    if (!current->enabled) {
        // CASO 1: Canale spento -> Accendi e dai focus
        current->enabled = 1;
        current->focused = 1;
        other->focused = 0;
        currentMenu = menu;

        
    } 
    else if (current->enabled && !current->focused) {
        // CASO 2: Acceso ma non a fuoco -> Sposta il focus qui
        current->focused = 1;
        other->focused = 0;
        currentMenu = menu;

    } 
    else {
        // CASO 3: Già a fuoco e acceso -> Spegni tutto
        current->enabled = 0;
        current->focused = 0;
        // 2. Se stiamo spegnendo il canale, puliamo lo schermo dai "fantasmi"

    for (uint16_t i = 0; i < 400; i++) {
        // Calcoliamo la X aggiungendo il margine (5)
        // Cancelliamo il pixel usando la Y memorizzata nel buffer vecchio
        tft_drawPixel(i + MARGIN_X, buffers_vecchi[idx][i], BLACK);
    }
    
        //close_menu();
    }
    
    // Forza il ridisegno dell'interfaccia grafica
    updateSidebarLabels();
}

void init_channels() {
    // CH1 Default
    ch1.enabled = 1;
    ch1.focused = 1;
    ch1.volts_div = 1.0;
    ch1.offset = 60; 
    ch1.coupling = COUPL_DC; // Aggiunto accoppiamento di default
    ch1.probe = 0; // Sonda 1X di default
    ch1.inverted = false; // Non invertito di default
    ch1.vdiv_idx = 6; // Indice per 1V/div
    ch1.color = YELLOW;
    ch1.bw_limit = false; // Limite banda disattivato di default
    ch1.multiplier = 1.0f; // Moltiplicatore sonda iniziale (1X)
    
    // CH2 Default
    ch2.enabled = 0;
    ch2.focused = 0;
    ch2.volts_div = 1.0;
    ch2.offset = 70; 
    ch2.coupling = COUPL_DC; // Aggiunto accoppiamento di default
    ch2.probe = 0; // Sonda 1X di default
    ch2.inverted = false; // Non invertito di default
    ch2.vdiv_idx = 6; // Indice per 1V/div
    ch2.color = CYAN;
    ch2.bw_limit = false; // Limite banda disattivato di default
    ch2.multiplier = 1.0f; // Moltiplicatore sonda iniziale (1X)
}

void updateChannelVoltDiv(Channel *ch, int16_t current_enc, int16_t *last_enc) {
    if (current_enc != *last_enc) {
        /*uart_print("Encoder CH");
        uart_print_int16(current_enc);
        uart_print("\r\n");*/
        if (!ch->isFine) {
            // --- COARSE: L'encoder (0-9) detta l'indice ---
            ch->vdiv_idx = (uint8_t)current_enc;
            ch->volts_div = v_div_values[ch->vdiv_idx];
        } 
        else {
            // --- FINE: Valore Assoluto Lineare ---
            // L'encoder sposta il valore a step di 0.01 fisso
            ch->volts_div = (float)current_enc / 100.0f;
        }

        *last_enc = current_enc;
    }
    /*uart_print("volts_div ");
        uart_print_float(ch->volts_div, 2);
        uart_print("\r\n");*/
}

// --- main loop ---
void scope_main(void)
{
    uint8_t key, rep;
    uint8_t new_sel;
    uint16_t new_trigger_level;
    
    init_channels();
    conf_encoder();
    drawStaticInterface();
    update_status_bar(true);
    set_base_time(11);
    set_trigger_level(trigger_level_12bit);   
    set_trigger_mode(TRIG_MODE_AUTO, TRIG_SLOPE_RISING, trigger_source);
    
    misure->source = 1; // Default su CH1
    misure->type = 0;   // Default su Vpp
    misure->active = 0;
    misure->f_active = 0;

   while(1)
    {
        pan_flag = false;
        uint8_t ev = 0xff;
        if (keypad_poll(&key, &rep)) {
            //ui_handle_key(key, rep);
            ev = key;

        } else {
            ev = 0xFF;
        }
        
        
        if(ev != 0xFF){
            uart_print("Event: ");
            uart_print_hex(ev);
            uart_print("\r\n");
           /* tft_fillRect(100, 100, 100, 16, BLACK);
        setTextColor(YELLOW, BLACK);
        tft_set_cursor(100, 100);
        tft_Print("CH2: ");
            tft_print_float(ev, 2);*/

            switch (ev)
            {
                // --- TASTI FISICI DEDICATI (Master) ---
                case 20: // Tasto PAN 
                    if(currentMenu == MENU_PAN) view_offset = 0; //Riporta in centro
                    else{
                        
                    }
                    break;
                case KEY_RUN:
                    if(trigger_mode == TRIG_MODE_SINGLE)
                        is_running = true;
                    else
                        is_running = !is_running;

                    if (is_running) {
                        trigger_mode = TRIG_MODE_AUTO;
                        set_trigger_mode(trigger_mode, trigger_slope, trigger_source);
                        freeze = false; // Se ripartiamo, sblocchiamo tutto
                        REG_TRIG = 0x01; // Forza un riarmo immediato della FSM
                    }
                    update_status_bar(true);
                    break;
                case KEY_SINGLE:
                    trigger_mode = TRIG_MODE_SINGLE;
                    set_trigger_mode(trigger_mode, trigger_slope, trigger_source);
                    freeze = false;     // Fondamentale: permette a osc_read_triggered di armare
                    is_running = true;  // Ci assicuriamo che il loop chiami l'acquisizione
                    REG_TRIG = 0x01;    // Armiamo la FSM FPGA
                    //updateSidebarLabels();
                    update_status_bar(true);
                    break;

                case KEY_TRIGGER: // Tasto fisico "Trigger"
                    currentMenu = MENU_TRIG;
                    updateSidebarLabels(); 
                    break;
                case 01: // Ipotetico tasto fisico "Measure"
                    currentMenu = MENU_MEAS;
                    updateSidebarLabels(); 
                    break;
                case KEY_CH1: // Tasto fisico "Vertical CH1"
                    handle_channel_button(1);
                    break;
                case KEY_CH2: // Tasto fisico "Vertical CH2"
                    handle_channel_button(2);
                    break;
                case 15: // Tasto encoder per la posizione verticale (Y-POS)
                    write_encoder(0, OFFSET_Y1_C_VAL); // Reset posizione Y CH1 
                    break;
                case 16: // Tasto encoder per la posizione verticale (Y-POS)
                    write_encoder(2, OFFSET_Y2_C_VAL); // Reset posizione Y CH2
                    break;
                case 17: // Tasto encoder per il livello di trigger
                    write_encoder(5, TRIG_C_VAL); // Reset base dei tempi
                    break;
                case 18: // Tasto encoder per il PAN
                    write_encoder(6, 0); // Reset livello di trigger
                    break;
            }
            switch (currentMenu){
                case MENU_CH1:
                    switch (ev) {
                        case 12: // Tasto 1 
                            cycleCoupling(&ch1);
                            break;

                        case 9:  // Tasto 2 
                            toggleBWLimit(&ch1);
                            break;
                        case 6:  // Tasto 3 
                            toggleFineCoarse(&ch1, &last_enc1);
                            break;
                        case 3:  // Tasto 3 
                            cycleProbe(&ch1);
                            last_enc1 = -1; // Forziamo il reset dell'encoder per evitare problemi di sincronizzazione quando si torna a CH1 dopo aver cambiato canale
                            break;

                        case 0:  // Tasto 4 
                            toggleInvert(&ch1);
                            break;

                    }
                    break;
                case MENU_CH2:
                    switch (ev) {
                        case 12: // Tasto 1 
                            cycleCoupling(&ch2);
                            break;

                        case 9:  // Tasto 2 
                            toggleBWLimit(&ch2);
                            break;
                        case 6:  // Tasto 3 
                            toggleFineCoarse(&ch2, &last_enc2);
                            break;
                        case 3:  // Tasto 3 
                            cycleProbe(&ch2);
                            last_enc2 = -1; // Forziamo il reset dell'encoder per evitare problemi di sincronizzazione quando si torna a CH2 dopo aver cambiato canale
                            break;

                        case 0:  // Tasto 4 
                            toggleInvert(&ch2);
                            break;

                    }
                    break;
                case MENU_MEAS:
                    switch (ev) {
                        case 12: // Tasto 1 (Top) -> Sorgente (CH1 / CH2)
                            misure->source++;
                            if (misure->source > 2) misure->source = 1;
                            updateSidebarLabels();
                            break;

                        case 9:  // Tasto 2 
                            misure->type++;
                            if (misure->type > 2) misure->type = 0;
                            updateSidebarLabels();
                            break;
                        case 6:  // Tasto 3 
                            misure->active = !misure->active;
                            updateSidebarLabels();
                            break;
                        case 3:  // Tasto 3 
                            misure->f_active = !misure->f_active;
                            updateSidebarLabels();
                            break;
                            break;

                        case 0:  // Tasto 4 
                            
                            break;

                    }
                    break;
                case MENU_TRIG:
                    switch (ev) {
                        case 12: // Tasto 1 (Top) -> Sorgente (CH1 / CH2)
                            trigger_source++;
                            if (trigger_source > 2) trigger_source = 1;
                            set_trigger_mode(trigger_mode, trigger_slope, trigger_source);
                            updateSidebarLabels();
                            break;

                        case 9:  // Tasto 2 -> Fronte (Rising / Falling)
                            trigger_slope = !trigger_slope;
                            set_trigger_mode(trigger_mode, trigger_slope, trigger_source);
                            updateSidebarLabels();
                            break;

                        case 6:  // Tasto 3 -> Modalità (AUTO / NORMAL)
                            trigger_mode++;
                            if (trigger_mode > 1) trigger_mode = 0;
                            set_trigger_mode(trigger_mode, trigger_slope, trigger_source);
                            updateSidebarLabels();
                            break;

                        case 3:  // Tasto 4
                            //toggleTrigLevelMode();
                            //updateSidebarLabels();
                            break;

                        case 0:  // Tasto 5 
                            //currentMenu = MENU_NONE; // O torna al menu precedente
                            //drawStaticInterface();   // Ridisegna tutto per pulire la sidebar
                            
                            //updateSidebarLabels();
                            break;
                    }
                    break;
            }

           
        }

    update_all_encoders();

    new_sel = encoder_values[4];
    if (new_sel != prev_time_div_sel) {
        // Aggiorna il registro solo se il valore è cambiato
        REG_BASETIME = new_sel;
        prev_time_div_sel = new_sel;
        time_div_sel = new_sel; // aggiorna il valore corrente
        time_div_sel_changed = true;
        current_time_base_idx =time_div_sel;
        freeze = false;     // Fondamentale: permette a osc_read_triggered di armare
                    is_running = true;  // Ci assicuriamo che il loop chiami l'acquisizione
                    REG_TRIG = 0x01;    // Armiamo la FSM FPGA
        updateSidebarLabels(); 
    }

    new_trigger_level = encoder_values[5];
    if (new_trigger_level != prev_trigger_level) {
        // Aggiorna
        trigger_level_12bit = new_trigger_level;
        set_trigger_level(trigger_level_12bit);
        prev_trigger_level = new_trigger_level;
        
        // Aggiorna la scritta della tensione in alto
        updateSidebarLabels(); 
    }

    int16_t new_pan = encoder_values[6];
    if (new_pan != prev_view_offset) {
        
        // Aggiorna il registro solo se il valore è cambiato
        osc_write_view_offset(new_pan);
        osc_arm_readout(); 
        prev_view_offset = new_pan;
        view_offset = new_pan; // aggiorna il valore corrente
        pan_flag = true;
        /*uart_print("offset ");
        uart_print_int16(view_offset);
        uart_print("\r\n");*/
    }

    ch1.offset = encoder_values[0]; // Aggiorna posizione Y CH1
    ch2.offset = encoder_values[2]; //
    //y_offset_ch[0] = encoder_values[0]; // Aggiorna posizione Y CH1
    //y_offset_ch[1] = encoder_values[2]; // Aggiorna posizione Y CH2

    //ch1.vdiv_idx = encoder_values[1]; // Aggiorna Volt/Div CH1
    //ch2.vdiv_idx = encoder_values[3]; // Aggiorna Volt/Div

    // Aggiorna Canale 1 (usa encoder_values[1])
    updateChannelVoltDiv(&ch1, encoder_values[1], &last_enc1);

    // Aggiorna Canale 2 (usa encoder_values[2] - o quello che hai assegnato)
    updateChannelVoltDiv(&ch2, encoder_values[3], &last_enc2);

    // 1. Leggiamo il valore attuale dell'encoder dedicato al verticale (es. encoder_values[1])
/*int16_t current_enc_v = encoder_values[1];
static int16_t last_enc_v = 0; // Serve per capire se l'encoder si è mosso

if (current_enc_v != last_enc_v) {
    // Determiniamo la direzione del movimento
    int8_t dir = (current_enc_v > last_enc_v) ? 1 : -1;
    last_enc_v = current_enc_v;

    if (!ch1.isFine) {
        // --- MODALITÀ COARSE (Step 1-2-5) ---
        int8_t new_idx = ch1.vdiv_idx + dir;
        
        // Limiti dell'array v_div_values (0-9)
        if (new_idx >= 0 && new_idx < 10) {
            ch1.vdiv_idx = new_idx;
            ch1.volts_div = v_div_values[ch1.vdiv_idx];
            // Qui potresti chiamare una funzione per ridisegnare solo l'etichetta nel menu
        }
    } 
    else {
        // --- MODALITÀ FINE (Variazione continua) ---
        // Aumentiamo o diminuiamo del 5% o di un valore fisso (es. 0.001)
        float step = ch1.volts_div * 0.05f; // Variazione del 5% per ogni scatto
        ch1.volts_div += (dir * step);

        // Limiti di sicurezza per il modo Fine (es. tra 10mV e 10V)
        if (ch1.volts_div < 0.01f) ch1.volts_div = 0.01f;
        if (ch1.volts_div > 10.0f) ch1.volts_div = 10.0f;
    }
}*/

    acquire_and_draw();
    update_status_bar(false);
        

    
    }
}
