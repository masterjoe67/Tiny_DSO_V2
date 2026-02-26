#include <stdint.h>
#include <util/delay.h>
#include <stdbool.h>
#include "Peripheral/st7798.h"
#include "Peripheral/input.h"
#include "Peripheral/uart.h"
#include "scope.h"



#define PRE_TRIGGER       200
#define POST_TRIGGER      200
#define BUFFER_TOTAL      (PRE_TRIGGER + POST_TRIGGER)

// Parametri reticolo
#define GRID_SPACING 30     // distanza tra linee
#define DOT_SPACING 4       // distanza tra puntini
#define COLOR_GRID WHITE

uint8_t buffer_a[BUFFER_TOTAL];
uint8_t buffer_b[BUFFER_TOTAL];

uint16_t old_buffer_a[400];
uint16_t old_buffer_b[400];


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

uint8_t currentMenu = MENU_CH1; // Default


//bool ch_visible[2] = {true, true}; 
uint16_t* buffers_vecchi[2] = {old_buffer_a, old_buffer_b}; // Puntatori ai tuoi buffer di cancellazione

uint16_t trigger_level_12bit = 0x07FF;

int16_t last_trig_y = -1; // Per cancellare la vecchia linea

uint8_t current_time_base_idx = 0;
static bool is_running= true;


const char* v_div_labels[] = {
    "10mV", "20mV", "50mV", 
    "100mV", "200mV", "500mV", 
    "1V", "2V", "5V","10V"
};

#define MAX_VDIV_IDX 9


uint8_t old_current_time_base_idx = 0xFF;

//uint8_t old_ch_coupling[2] = {0xFF, 0xFF} ;
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

void set_base_time(uint8_t index) {
    // Limite di sicurezza a 1s (indice 18)
    if (index > MAX_TIMEBASE_IDX) index = MAX_TIMEBASE_IDX;
    
    current_time_base_idx = index;

    // Scrittura nel registro MMIO dell'FPGA
    REG_BASETIME = index;
    
    // Aggiorna la grafica
   // update_timebase_ui();
}

//Prototipe
void draw_trigger_line(uint16_t level12, uint16_t color, bool erase);


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


// funzione per disegnare la traccia sul TFT
/*void draw_trace(uint8_t *buffer, int16_t *old_buffer, uint16_t length, int16_t y_offset, uint16_t color, bool inverted, bool enabled)
{
    const int16_t Y_MIN = MARGIN_Y;
    const int16_t Y_MAX = MARGIN_Y + TRACE_H;

    for (uint16_t i = 0; i < length; i++) {
        uint16_t x = i + MARGIN_X;

        // 1. Cancellazione sempre attiva (se c'era qualcosa di vecchio)
        if (old_buffer[i] > Y_MIN && old_buffer[i] < Y_MAX) {
            tft_drawPixel(x, old_buffer[i], BLACK);
        }

        // 2. Calcolo e disegno solo se il canale è abilitato
        if (enabled) {
            uint8_t raw_data = buffer[i];
            if (!inverted) raw_data = 255 - raw_data;

            int16_t y_now = (int16_t)(raw_data / 2) + y_offset;

            // Disegno se nei limiti
            if (y_now > Y_MIN && y_now < Y_MAX) {
                tft_drawPixel(x, y_now, color);
            }
            // Memorizzo la nuova posizione
            old_buffer[i] = y_now;
        } else {
            // Se disattivato, "resetto" il vecchio buffer a un valore fuori schermo
            // Così al prossimo giro non tenterà di cancellare nulla
            old_buffer[i] = -100; 
        }
    } 
}*/
void draw_trace(uint8_t *buffer, int16_t *old_buffer, uint16_t length, int16_t y_offset, uint16_t color, bool inverted, bool enabled, bool vectors)
{
    const int16_t Y_MIN = MARGIN_Y;
    const int16_t Y_MAX = MARGIN_Y + TRACE_H;
    
    int16_t y_prev_new = -100; // Memorizza la Y del punto precedente (nuova traccia)
    int16_t y_prev_old = -100; // Memorizza la Y del punto precedente (vecchia traccia per cancellazione)

    for (uint16_t i = 0; i < length; i++) {
        uint16_t x = i + MARGIN_X;

        // --- 1. CANCELLAZIONE ---
        if (old_buffer[i] > Y_MIN && old_buffer[i] < Y_MAX) {
            if (vectors && i > 0 && y_prev_old > Y_MIN && y_prev_old < Y_MAX) {
                // Cancella il vettore precedente
                tft_drawLine(x - 1, y_prev_old, x, old_buffer[i], BLACK);
            } else {
                // Cancella il punto singolo
                tft_drawPixel(x, old_buffer[i], BLACK);
            }
        }
        y_prev_old = old_buffer[i];

        // --- 2. DISEGNO ---
        if (enabled) {
            uint8_t raw_data = buffer[i];
            if (!inverted) raw_data = 255 - raw_data;

            int16_t y_now = (int16_t)(raw_data / 2) + y_offset;

            if (y_now > Y_MIN && y_now < Y_MAX) {
                if (vectors && i > 0 && y_prev_new > Y_MIN && y_prev_new < Y_MAX) {
                    // Disegna vettore dal punto precedente a quello attuale
                    tft_drawLine(x - 1, y_prev_new, x, y_now, color);
                } else {
                    // Disegna punto singolo
                    tft_drawPixel(x, y_now, color);
                }
            }
            
            y_prev_new = y_now;
            old_buffer[i] = y_now; // Memorizza per il prossimo frame
        } else {
            old_buffer[i] = -100; 
        }
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
    if (trigger_mode == TRIG_MODE_SINGLE & freeze){
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

void osc_read_triggered2(uint8_t *a, uint8_t *b)
{
    /* 1. Gestione del blocco acq */
    if (freeze) {
        // Se siamo in freeze (Single finito o Stop), leggiamo solo i vecchi dati
        osc_arm_readout(); 
    } else {
        // Aspetta che l'FPGA scatti (Attenzione: osc_wait_ready deve essere non-bloccante 
        // o avere un timeout se vuoi che i tasti rispondano subito!)
        osc_wait_ready();

        if (trigger_mode == TRIG_MODE_SINGLE) {
            freeze = true; // Prossimo giro sarà bloccato
        }
        
        osc_arm_readout(); // Avvia trasferimento dati da FPGA a AVR
    }

    /* 2. Trasferimento dati */
    for (int i = 0; i < 400; i++) {
        b[i] = REG_CHB;
        a[i] = REG_CHA;
    }

    /* 3. Riarmo Trigger: Solo se RUNNING e NON in modalità Single appena conclusa */
    if (is_running && !(trigger_mode == TRIG_MODE_SINGLE && freeze)) {
        REG_TRIG = 0x01;
    }
}
void osc_read_triggered(uint8_t *a, uint8_t *b)
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

    /* 4. Trasferimento dati (Dura pochi microsecondi a 60MHz) */
    for (int i = 0; i < 400; i++) {
        b[i] = REG_CHB;
        a[i] = REG_CHA;
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


/*void draw_ground_marker(uint8_t channel_idx, uint16_t color) {
    // 1. Calcoliamo la posizione Y dello zero (valore ADC 128)
    // Usiamo la stessa identica formula della tua draw_trace
    int16_t y_zero = 64 + y_offset_ch[channel_idx];

    // 2. Cancelliamo il vecchio marker (opzionale, o cancelli l'intera colonna prima)
    if(y_offset_ch[channel_idx] != old_y_offset_ch[channel_idx]){
        tft_FillTriangle(gnd_mark_a[channel_idx], gnd_mark_b[channel_idx], gnd_mark_c[channel_idx], BLACK);
    }
    // tft_fillRect(0, MARGIN_Y, MARGIN_X - 2, TRACE_H, BLACK);

    // 3. Disegniamo un piccolo triangolo puntato a destra
    // Vertici: (x,y), (x,y), (x,y)
    // Lo mettiamo subito a sinistra della griglia (MARGIN_X)
    uint16_t x_tip = MARGIN_X + 8;
    uint16_t x_base = MARGIN_X;

    gnd_mark_a[channel_idx].x = x_base;
    gnd_mark_a[channel_idx].y = y_zero - 5;
    gnd_mark_b[channel_idx].x = x_base;
    gnd_mark_b[channel_idx].y = y_zero + 5;
    gnd_mark_c[channel_idx].x = x_tip;
    gnd_mark_c[channel_idx].y = y_zero;
    // Disegno del triangolino pieno
    tft_FillTriangle(gnd_mark_a[channel_idx], gnd_mark_b[channel_idx], gnd_mark_c[channel_idx], color);     // Punta (che indica lo zero sulla griglia)color

    // Opzionale: Scriviamo il numero del canale "1" o "2" dentro o vicino
    //setTextColor(color, BLACK);
    //tft_set_cursor(x_base + 1, y_zero - 3);
    //tft_print_int(channel_idx + 1);
    drawPanTrack();
}*/

void draw_ground_marker2(Channel *ch) {
    // 1. Calcoliamo la posizione Y usando l'offset contenuto nella struct
    // y_zero = centro dello schermo (es. 64) + offset del canale
    int16_t y_zero = 64 + ch->offset;

    // 2. Gestione della cancellazione (se l'offset è cambiato)
    // Nota: qui dovresti aggiungere 'old_offset' alla struct Channel 
    // per un confronto preciso, oppure cancellare prima di aggiornare.
    if(ch->offset != ch->old_offset) {
        tft_FillTriangle(ch->gnd_mark_a, ch->gnd_mark_b, ch->gnd_mark_c, BLACK);
    }

    // 3. Coordinate del marker
    // Usiamo le variabili interne alla struct per i vertici
    uint16_t x_tip = MARGIN_X + 8;
    uint16_t x_base = MARGIN_X;

    ch->gnd_mark_a.x = x_base;
    ch->gnd_mark_a.y = y_zero - 5;
    
    ch->gnd_mark_b.x = x_base;
    ch->gnd_mark_b.y = y_zero + 5;
    
    ch->gnd_mark_c.x = x_tip;
    ch->gnd_mark_c.y = y_zero;

    // 4. Disegno del triangolo
    // Se il canale è a fuoco (focused), potresti disegnarlo più luminoso 
    // o con un bordo bianco per imitare il Tek!
    uint16_t draw_color = ch->color;
    if (!ch->focused) {
        // Se non ha il focus, rendiamo il colore un po' più scuro/spento
        // (Esempio rudimentale di bit-shift per scurire l'RGB565)
        draw_color = (ch->color >> 1) & 0x7BEF; 
    }

    tft_FillTriangle(ch->gnd_mark_a, ch->gnd_mark_b, ch->gnd_mark_c, draw_color);

    // 5. Opzionale: Numero del canale accanto al marker
   /* if (ch->focused) {
        tft_setTextColor(WHITE, BLACK); // Bianco se selezionato
    } else {
        tft_setTextColor(draw_color, BLACK);
    }
    
    // Posizioniamo il cursore e stampiamo il numero (1 o 2)
    // ch == &ch1 ? 1 : 2
    tft_set_cursor(x_base + 1, y_zero - 4);
    tft_print_int(ch == &ch1 ? 1 : 2);*/

    drawPanTrack();
}


void acquire_and_draw(){
    // 1. ACQUISIZIONE (Condizionale)
    // Proviamo a leggere solo se siamo in RUN o in un SINGLE attivo
    //if (is_running || (trigger_mode == TRIG_MODE_SINGLE && !freeze)) {
    if (is_running || (trigger_mode == TRIG_MODE_SINGLE && !freeze) || pan_flag) {
        osc_read_triggered(buffer_a, buffer_b);
    }

    // 2. DISEGNO (Sempre attivo!)
    // Da qui in poi, il codice deve girare SEMPRE, anche in STOP.
    // Solo così il PAN può funzionare sui dati vecchi.
    
    tft_drawGrid(LIGHTGREY);

    // Disegna CH1 (buffer_a contiene l'ultima cattura, il pan lo sposta dentro draw_trace)
    draw_trace(buffer_a, old_buffer_a, 400, ch1.offset, ch1.color, ch1.inverted, ch1.enabled , true);
    
    // Disegna CH2
    draw_trace(buffer_b, old_buffer_b, 400, ch2.offset, ch2.color, ch2.inverted, ch2.enabled, true);
    
    // UI e Marker (Sempre visibili per poterli muovere in STOP)
    draw_trigger_line(trigger_level_12bit, YELLOW, false);
    draw_ground_marker2(&ch1);
    draw_ground_marker2(&ch2);
    /*draw_ground_marker(0, GREEN);
    draw_ground_marker(1, RED);*/
}

void drawMenuButton(uint8_t index, const char* label, bool active, uint16_t color) {
    uint16_t y = 25 + (index * 50); // Calcola posizione Y in base all'indice
    uint16_t bgColor = BLACK;       // Definiamo lo sfondo fisso a nero
    
    // 1. Disegna la cornice del bottone
    tft_drawRect(410, y, 65, 40, color);
    
    // 2. Gestione del bordo "attivo" per dare spessore
    /*if (active) {
        tft_drawRect(409, y-1, 67, 42, color); 
    } else {
        // Se non è attivo, cancelliamo il bordo esterno con lo sfondo
        tft_drawRect(409, y-1, 67, 42, bgColor);
    }*/

    // 3. Scrivi il testo passando tutti i parametri richiesti dalla tua funzione
    // Usiamo 'color' per il testo e 'bgColor' per il fondo del carattere
    tft_printAt(label, 415, y + 15, color, bgColor);
}



void drawStaticInterface() {
    // 1. Pulisce tutto lo schermo
    tft_fillScreen(BLACK);
    
    // 2. Barra Superiore (Status e Misure rapide)
    tft_fillRect(0, 0, 480, 20, DARKGREY);
    tft_printAt("Mje", 10, 5, GREEN, DARKGREY);
    //tft_printAt("T: 100uS", 120, 5, WHITE, DARKGREY);
    //tft_printAt("Vpp: 3.24V", 250, 5, YELLOW, DARKGREY);

    // --- TITOLO MENU A DESTRA (Sopra i tasti) ---
    const char* menuName;
    if (currentMenu == MENU_CH1)      menuName = "CH 1";
    else if (currentMenu == MENU_CH2) menuName = "CH 2";
    else if (currentMenu == MENU_TRIG) menuName = "TRIG";
    else                              menuName = "MENU";
    
    tft_printAt(menuName, 430, 5, WHITE, DARKGREY);

    // 3. Cornice Area Traccia (400x240)
    tft_drawRect(MARGIN_X - 1, MARGIN_Y - 1, TRACE_W + 2, TRACE_H + 2, WHITE);
    
    // 4. Linea di divisione Sidebar
    tft_drawLine(SIDEBAR_X - 2, 20, SIDEBAR_X - 2, TRACE_H + MARGIN_Y, GREY);

    // 5. Disegno dei 5 Soft-Keys 
    /*if (currentMenu == MENU_CH1 || currentMenu == MENU_CH2) {
        uint8_t idx = (currentMenu == MENU_CH1) ? 0 : 1;
        
        // Etichette dinamiche per accoppiamento e sonda
        const char* coupLbl = (ch_coupling[idx] == COUPL_DC) ? "DC" : (ch_coupling[idx] == COUPL_AC ? "AC" : "GND");
        const char* probLbl = (ch_probe[idx] == 0) ? "1X" : (ch_probe[idx] == 1 ? "10X" : "100X");

        //drawMenuButton(0, ch_visible[idx] ? (idx==0?"CH1 ON":"CH2 ON") : (idx==0?"CH1 OFF":"CH2 OFF"), true, WHITE);
        drawMenuButton(0, "nn", true, WHITE);
        drawMenuButton(1, coupLbl, false, WHITE);
        drawMenuButton(2, probLbl, false, WHITE);
        drawMenuButton(3, ch1.inverted ? "-INV-" : "INVERT", false, WHITE);
        //drawMenuButton(4, (encoderMode == MODE_Y_POS) ? "> POS <" : "POSITION", false, WHITE);
    }*/
    /*if (currentMenu == MENU_CH1 || currentMenu == MENU_CH2) {
    // 1. Identifichiamo il canale corrente tramite puntatore
    Channel *ch = (currentMenu == MENU_CH1) ? &ch1 : &ch2;
    
    // 2. Etichette dinamiche (Molto più leggibili leggendo dalla struct)
    const char* coupLbl = (ch->coupling == COUPL_DC) ? "DC" : 
                          (ch->coupling == COUPL_AC) ? "AC" : "GND";
                          
    const char* probLbl = (ch->probe == 0) ? "1X" : 
                          (ch->probe == 1) ? "10X" : "100X";

    // 3. Disegno dei bottoni (Stile Tektronix)
    
    // TASTO 0: Stato visibilità (On/Off)
    //drawMenuButton(0, ch->enabled ? "ON" : "OFF", ch->enabled, WHITE);
    
    // TASTO 1: Accoppiamento
    drawMenuButton(0, coupLbl, false, WHITE);
    

    // TASTO 2: Sonda
    drawMenuButton(2, probLbl, false, WHITE);
    
    // TASTO 3: Inversione (Usiamo il campo della struct)
    drawMenuButton(3, ch->inverted ? "-INV-" : "INVERT", ch->inverted, WHITE);
    
    // TASTO 4: Posizione (Indichiamo quale canale stiamo muovendo col suo colore!)
    drawMenuButton(4, "POSITION", ch->focused, ch->color);
}
    else if (currentMenu == MENU_TRIG) {
        drawMenuButton(0, (trigger_mode == 0) ? "AUTO" : "NORMAL", true, WHITE);
        drawMenuButton(1, trigger_slope ? "RISE" : "FALL", false, WHITE);
        drawMenuButton(2, (trigger_source == 1) ? "CH1" : "CH2", false, WHITE);
        //drawMenuButton(3, (encoderMode == MODE_TRIG_LEVEL) ? "> LEV <" : "LEVEL", false, WHITE);
        //drawMenuButton(4, "BACK", false, WHITE);
    }*/
    updateSidebarLabels(); // Aggiorna tutte le etichette in base allo stato attuale (usa i dati nelle struct)
    // 6. Ripristina la griglia
    tft_drawGrid(LIGHTGREY);
}

/*void toggleCH(uint8_t ch) 
{
    // ch: 1 per CH1, 2 per CH2
    uint8_t idx = ch - 1;

    // 1. Inverte lo stato di visibilità
    ch_visible[idx] = !ch_visible[idx];

    // 2. Se stiamo spegnendo il canale, puliamo lo schermo dai "fantasmi"
    if (!ch_visible[idx]) {
        for (uint16_t i = 0; i < 400; i++) {
            // Calcoliamo la X aggiungendo il margine (5)
            // Cancelliamo il pixel usando la Y memorizzata nel buffer vecchio
            tft_drawPixel(i + MARGIN_X, buffers_vecchi[idx][i], BLACK);
        }
    }

    // 3. Aggiorna la sidebar per riflettere il nuovo stato
    // Questa funzione userà internamente ch_visible[idx] per scrivere "ON" o "OFF"
    updateSidebarLabels(); 
}*/


/*void cycleCoupling(uint8_t ch) 
{
    // ch deve essere 1 per CH1 e 2 per CH2
    uint8_t idx = ch - 1; 

    // 1. Cicla tra 0, 1 e 2 per il canale selezionato (DC, AC, GND)
    ch_coupling[idx]++;
    if (ch_coupling[idx] > COUPL_GND) {
        ch_coupling[idx] = COUPL_DC;
    }

    // 2. Comunicazione Hardware (decommenta quando sei pronto)
    // inviaComandoHardware(ch, ch_coupling[idx]);

    // 3. Preparazione etichetta
    const char* label;
    switch(ch_coupling[idx]) {
        case COUPL_DC:  label = "DC ";  break;
        case COUPL_AC:  label = "AC ";  break;
        case COUPL_GND: label = "GND"; break;
        default:        label = "??";  break;
    }
    
    // 4. Feedback visivo
    // Determiniamo il colore in base al canale per uno "spettacolo" perfetto
    //uint16_t color = (ch == 1) ? YELLOW : CYAN;
    uint16_t color = WHITE;
    // Ridisegna il bottone (il tasto 9 corrisponde all'indice 1 della sidebar)
    // Ora passiamo correttamente label, stato active e colore
    drawMenuButton(1, label, true, color); 
}*/
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
    const char* label;
    switch(ch->coupling) {
        case COUPL_DC:  label = "DC ";  break;
        case COUPL_AC:  label = "AC ";  break;
        case COUPL_GND: label = "GND";  break;
        default:        label = "??";   break;
    }
    
    // 4. Feedback visivo
    // Usiamo il colore contenuto nella struct (YELLOW o CYAN/BLUE)
    // per far capire subito all'utente su quale canale sta agendo.
    //uint16_t color = ch->color; 

    // Ridisegna il bottone (indice 1 della sidebar)
    // Passiamo la label, true per indicare che è "attivo" e il colore del canale
    drawMenuButton(0, label, true, WHITE); 
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
    const char* label = ch->bw_limit ? "20MHz" : "FULL ";
    
    // 4. Feedback visivo
    // Usiamo il colore del canale per evidenziare quando il filtro è attivo
    //uint16_t color = ch->bw_limit ? ch->color : WHITE;

    // Supponiamo di usare il tasto 4 della sidebar per il BW Limit
    drawMenuButton(1, label, ch->bw_limit, WHITE); 
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
    const char* label;
    switch(ch->probe) {
        case 0:  label = "1X  "; break;
        case 1:  label = "10X "; break;
        case 2:  label = "100X"; break;
        default: label = "??  "; break;
    }
    
    // 4. Aggiornamento grafico con il colore del canale
    // Usiamo ch->color così se sei nel menu CH1 è Giallo, se CH2 è Ciano
    //uint16_t color = ch->color;

    // Disegniamo il bottone (indice 2 della sidebar per il tasto Probe)
    drawMenuButton(3, label, true, WHITE); 
}

float calcolaVoltReali(Channel *ch, uint16_t valoreADC) {
    // 1. Converti il valore ADC (0-4095) in tensione.
    // Supponendo che il range di ingresso dell'ADC sia 5V (o quello del tuo front-end)
    // Usiamo 4095.0f per forzare il calcolo in virgola mobile.
    float v_letta = (valoreADC * 5.0f) / 4095.0f;
    
    // 2. Applica il moltiplicatore della sonda (1X, 10X, 100X) contenuto nella struct
    return v_letta * ch->multiplier;
}


void updateSidebarLabels() {
    // --- 1. AGGIORNAMENTO NOME MENU NELLA BARRA SUPERIORE ---
    const char* menuTitle;
    uint16_t menuColor; // Variabile per il colore del titolo
    switch (currentMenu) {
        case MENU_CH1:
            menuTitle = " CH 1 ";
            menuColor = GREEN;  // Colore traccia 1
            break;
            
        case MENU_CH2:
            menuTitle = " CH 2";
            menuColor = RED;    // Colore traccia 2
            break;
            
        case MENU_TRIG:
            menuTitle = " TRIG ";
            menuColor = YELLOW; // Colore linea trigger
            break;
            
        /*case MENU_TBASE:
            menuTitle = "T-BASE";
            menuColor = WHITE;
            break;
        
        case MENU_PAN:
            menuTitle = " PAN  ";
            menuColor = MAGENTA;
            break;*/
            
        default:
            menuTitle = " MENU ";
            menuColor = CYAN;
            break;
    }
    
    // Scriviamo il titolo a destra (X=410) sopra i tasti
    tft_printAt(menuTitle, 425, 5, menuColor, DARKGREY);

    // --- 2. LOGICA TASTI SIDEBAR ---
    /*if (currentMenu == MENU_CH1 || currentMenu == MENU_CH2) {
        uint8_t chIdx = (currentMenu == MENU_CH1) ? 0 : 1;

        // TASTO 0: Stato ON/OFF
        if (ch_visible[chIdx]) {
            drawMenuButton(0, (chIdx == 0) ? "CH1 ON" : "CH2 ON", true, WHITE);
        } else {
            drawMenuButton(0, (chIdx == 0) ? "CH1 OFF" : "CH2 OFF", false, WHITE);
        }

        // TASTO 1: Accoppiamento
        const char* couplLabels[] = {"DC", "AC", "GND"};
        drawMenuButton(1, couplLabels[ch_coupling[chIdx]], true, WHITE);

        // TASTO 2: Sonda
        const char* probeLabels[] = {"1X", "10X", "100X"};
        drawMenuButton(2, probeLabels[ch_probe[chIdx]], true, WHITE);

        // TASTO 3: Inversione (Visualizziamo se è attiva)
        drawMenuButton(3, ch_inverted[chIdx] ? "-INV-" : "INVERT", ch_inverted[chIdx], WHITE);

        // TASTO 4: Posizione Y (Visualizziamo se l'encoder la sta controllando)
        //drawMenuButton(4, (encoderMode == MODE_Y_POS) ? "> POS <" : "POSITION", (encoderMode == MODE_Y_POS), WHITE);
    } */

    if (currentMenu == MENU_CH1 || currentMenu == MENU_CH2) {
        // 1. Puntatore al canale basato sul menu aperto
        Channel *ch = (currentMenu == MENU_CH1) ? &ch1 : &ch2;
        const char* chName = (currentMenu == MENU_CH1) ? "CH1" : "CH2";

        // TASTO 0: Accoppiamento (Aggiungi 'coupling' alla struct!)
        // 0: DC, 1: AC, 2: GND
        const char* couplLabels[] = {"DC", "AC", "GND"};
        drawMenuButton(0, couplLabels[ch->coupling], true, WHITE);

        // TASTO 1: Limite banda (Non implementato, mettiamo un placeholder)
        drawMenuButton(1, "BW: 20MHz", false, WHITE);

        // TASTO 2: Volt/div (Focus sull'encoder, ma mostriamo anche il moltiplicatore della sonda)
        char vdivLabel[10];
        sprintf(vdivLabel, "V/DIV: %.1f", ch->volts_div);
        drawMenuButton(2, vdivLabel, false, WHITE);

        // TASTO 3: Sonda (Aggiungi 'probe' alla struct!)
        const char* probeLabels[] = {"1X", "10X", "100X"};
        drawMenuButton(3, probeLabels[ch->probe], true, WHITE);

        // TASTO 3: Inversione (Usiamo ch->inverted)
        drawMenuButton(4, ch->inverted ? "-INV-" : "INVERT", ch->inverted, WHITE);

    }
    
    else if (currentMenu == MENU_TRIG) {
        // TASTO 0: Modalità (AUTO/NORMAL) - Usiamo i nuovi nomi
        //(0, (trigger_mode == 0) ? "AUTO  " : "NORM  ", true, WHITE);
        drawMenuButton(0, (trigger_source == 1) ? "SRC: CH1" : "SRC: CH2", true, WHITE);
        // TASTO 1: Slope (RISE/FALL) - Usiamo i nuovi nomi
        //(1, (trigger_slope == 1) ? "RISE" : "FALL", true, WHITE);
        drawMenuButton(1, trigger_slope ? "SLP: RISE" : "SLP: FALL", true, WHITE);
        // TASTO 2: Sorgente (CH1/CH2) - Usiamo i nuovi nomi
        //drawMenuButton(2, (trigger_source == 1) ? "SRC: CH1" : "SRC: CH2", true, WHITE);
        drawMenuButton(2, (trigger_mode == 0) ? "MODE: AUTO" : "MODE: NORM", true, WHITE);
        // TASTO 3: Livello (LEVEL)
        //drawMenuButton(3, (encoderMode == MODE_TRIG_LEVEL) ? "> LEV <" : "LEVEL", (encoderMode == MODE_TRIG_LEVEL), WHITE);

        // TASTO 4: Ritorno
        drawMenuButton(4, "BACK", false, WHITE);
    }

    else if (currentMenu == MENU_TBASE) {
        
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
  
    }
}

/*void toggleInvert(uint8_t ch) 
{
    // Indice array (0 per CH1, 1 per CH2)
    uint8_t idx = ch - 1; 

    // 1. Inverte lo stato booleano
    ch_inverted[idx] = !ch_inverted[idx];

    // 2. Comunicazione Hardware/VHDL
    // Se la gestione è nell'FPGA, invii il comando. 
    // Se è software, la funzione di disegno userà ch_inverted[idx].
    // inviaComandoInversione(ch, ch_inverted[idx]);

    // 3. Preparazione etichetta per la sidebar
    // Se invertito, scriviamo "INV" in grassetto o cambiamo etichetta
    const char* label = ch_inverted[idx] ? "-INV-" : "INVERT";
    
    // 4. Feedback visivo
    //uint16_t color = (ch == 1) ? YELLOW : CYAN;
    uint16_t color = WHITE;
    // Il tasto fisico 3 (ev 3) corrisponde al quarto bottone (indice 3)
    drawMenuButton(3, label, ch_inverted[idx], color); 
}*/
void toggleInvert(Channel *ch) 
{
    // 1. Inverte lo stato booleano direttamente nella struct
    ch->inverted = !ch->inverted;

    // 2. Comunicazione Hardware (Opzionale)
    // Se la tua FPGA ha un registro per l'inversione hardware:
    // set_fpga_inversion(ch == &ch1 ? 0 : 1, ch->inverted);

    // 3. Preparazione etichetta
    const char* label = ch->inverted ? "-INV-" : "INVERT";
    
    // 4. Feedback visivo
    // Usiamo il colore del canale per il tasto se è attivo, 
    // così l'utente ha un feedback immediato (Stile Tektronix)
    //uint16_t buttonColor = ch->inverted ? ch->color : WHITE;

    // Disegniamo il pulsante (Tasto 3 nel menu)
    drawMenuButton(4, label, ch->inverted, WHITE); 

}

int16_t scale_8bit_to_pixel(uint8_t raw_8bit, uint8_t vdiv_idx) {
    // 1. Centriamo il campione (0-255) rispetto allo zero virtuale (128)
    // Usiamo un int16 per gestire i valori negativi
    int16_t sample = (int16_t)raw_8bit - 128;

    // 2. Tabella dei fattori di scala
    // Se a 1V/div (idx 6) vogliamo che una divisione (30px) sia, ad esempio, 50 unità ADC
    // allora moltiplichiamo per un fattore che adatti il segnale.
    
    float scale_factor = 1.0f;
    switch(vdiv_idx) {
        case 0: scale_factor = 10.0f; break; // 10mV - Molto zoomato
        case 1: scale_factor = 5.0f;  break; // 20mV
        case 2: scale_factor = 2.0f;  break; // 50mV
        case 3: scale_factor = 1.0f;  break; // 100mV
        case 4: scale_factor = 0.5f;  break; // 200mV
        case 5: scale_factor = 0.2f;  break; // 500mV
        case 6: scale_factor = 0.1f;  break; // 1V - Rimpicciolito
        case 7: scale_factor = 0.05f; break; // 2V
        case 8: scale_factor = 0.02f; break; // 5V
        case 9: scale_factor = 0.01f; break; // 10V
    }

    // 3. Calcolo della coordinata Y
    // yCenter è il centro della tua griglia (es. 120 + MARGIN_Y)
    // Sottraiamo perché sul display l'asse Y è invertito (0 è in alto)
    int16_t y_pixel = ch1.offset - (int16_t)(sample * scale_factor);

    // 4. Clipping di sicurezza per non uscire dalla griglia
    if (y_pixel < MARGIN_Y) return MARGIN_Y;
    if (y_pixel > MARGIN_Y + TRACE_H) return MARGIN_Y + TRACE_H;

    return y_pixel;
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

        if (period > 0) {
            float freq = 3840000000.0f / (float)period;
            // Aggiorna il valore a video
        }
    } else if (freeze) {
        // In STOP, non ricalcolare: mantieni l'ultimo valore valido a schermo
        // così la cifra non sballa mentre ti muovi nella traccia.
    }
}

void draw_trigger_line(uint16_t level12, uint16_t color, bool erase) {
    // 1. Portiamo a 8 bit (0-255)
    uint8_t raw_data = level12 >> 4; 
    Channel *trig_ch = (trigger_source == 1) ? &ch1 : &ch2;
    // 2. Applichiamo la STESSA inversione della traccia
    // Se la traccia è invertita (inverted=false nel draw_trace), facciamo 255 - raw_data
    // Supponiamo che 'inverted' qui segua la stessa logica del canale selezionato
    bool ch_inverted = false; // Metti qui la variabile che passi a draw_trace per quel canale
    if (!ch_inverted) {
        raw_data = 255 - raw_data;
    }

    // 3. Calcolo coordinata Y reale (IDENTICO alla draw_trace)
    // Usiamo y_offset_ch che passi alla draw_trace
    //int16_t y = (int16_t)(raw_data / 2) + y_offset_ch[trigger_source - 1];
    int16_t y = (int16_t)(raw_data / 2) + trig_ch->offset;

    // 4. CANCELLAZIONE
    if (last_trig_y >= MARGIN_Y && last_trig_y <= (MARGIN_Y + TRACE_H)) {
        tft_drawFastHLine(MARGIN_X, last_trig_y, TRACE_W, BLACK);
        // Se vuoi essere pignolo, qui potresti ripristinare i puntini della griglia
    }

    if (!erase) {
        // 5. DISEGNO E CLIPPING
        if (y > MARGIN_Y && y < (MARGIN_Y + TRACE_H)) {
            // Disegno linea tratteggiata
            for (uint16_t x = MARGIN_X; x < MARGIN_X + TRACE_W; x += 10) {
                tft_drawFastHLine(x, y, 5, color); 
            }
            last_trig_y = y;
        } else {
            last_trig_y = 0; 
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
    if(ch->old_vdiv_idx != ch->vdiv_idx || ch->old_coupling != ch->coupling || force) {
        
        // Pulizia area (100px larghezza, 16px altezza)
        tft_fillRect(xPos, yPos, 100, 16, BLACK);
        
        // Colore del canale (Giallo per CH1, Ciano per CH2)
        setTextColor(ch->color, BLACK);
        tft_set_cursor(xPos, yPos);
        
        // Stampa Etichetta (CH1 o CH2 basandosi sull'indirizzo di memoria)
        tft_Print(ch == &ch1 ? "CH1: " : "CH2: ");
        
        // Stampa Valore V/div
        tft_Print(v_div_labels[ch->vdiv_idx]); 
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
        tft_printAt(label, 100, 5, color, DARKGREY);
        last_ui_state = current_state;
    }

    // --- CANALE 1 ---
   /* if(old_ch1_vdiv_idx != ch1_vdiv_idx || old_ch_coupling[0] != ch_coupling[0] || force){
        tft_fillRect(xStart, yPos, 100, 16, BLACK);
        setTextColor(CYAN, BLACK);
        tft_set_cursor(xStart, yPos);
        tft_Print("CH1: ");
        tft_Print(v_div_labels[ch1_vdiv_idx]); // es. "1V"
        tft_Print(" ");
        tft_Print(ch_coupling[0] ? "AC" : "DC");
        old_ch1_vdiv_idx = ch1_vdiv_idx;
        old_ch_coupling[0] = ch_coupling[0];
    }

    // --- CANALE 2 ---
    if(old_ch2_vdiv_idx != ch2_vdiv_idx || old_ch_coupling[1] != ch_coupling[1] || force){
        tft_fillRect(xStart + 100, yPos, 100, 16, BLACK);
        setTextColor(YELLOW, BLACK);
        tft_set_cursor(xStart + 100, yPos);
        tft_Print("CH2: ");
        tft_Print(v_div_labels[ch2_vdiv_idx]);
        tft_Print(" ");
        tft_Print(ch_coupling[1] ? "AC" : "DC");
        old_ch2_vdiv_idx = ch2_vdiv_idx;
        old_ch_coupling[1] = ch_coupling[1];
    }*/

    // Aggiorna info CH1
    draw_channel_status(&ch1, xStart, yPos, force);

    // Aggiorna info CH2 (spostato di 100 pixel a destra)
    draw_channel_status(&ch2, xStart + 100, yPos, force);

    // --- BASE TEMPI ---
    if(old_current_time_base_idx != current_time_base_idx || force){
        tft_fillRect(xStart + 210, yPos, 100, 16, BLACK);
        setTextColor(WHITE, BLACK);
        tft_set_cursor(xStart + 210, yPos);
        tft_Print("T: ");
        tft_Print(time_base_labels[current_time_base_idx]);
        old_current_time_base_idx = current_time_base_idx;
    tft_Print("/div");
    }

    // --- TRIGGER LEVEL ---
    if(old_trigger_level_12bit != trigger_level_12bit || force){
        tft_fillRect(xStart + 310, yPos, 100, 16, BLACK);
        setTextColor(GREEN, BLACK);
        tft_set_cursor(xStart + 310, yPos);
        tft_Print("Trig: ");
        // Calcoliamo il valore in Volt o mostriamo i bit
        // Se reg_trig_level è 0-4095 (12 bit)
        uint16_t level_mv = (uint32_t)trigger_level_12bit * 3300 / 4096; 
        tft_print_float(level_mv / 1000.0, 2);
        tft_Print("V");
        old_trigger_level_12bit = trigger_level_12bit;
    }
    // Supponiamo di aver calcolato 'freq'
    float freq = read_fpga_frequency();
    if(old_freq != freq){
        tft_set_cursor(MARGIN_X + 210, yPos + 20); // Una riga sotto la T/div
        setTextColor(WHITE, BLACK);
        tft_Print("F:");
        
        if (freq > 1000000) {
            tft_print_float(freq / 1000000.0, 2);
            tft_Print("MHz");
        } else if (freq > 1000) {
            tft_print_float(freq / 1000.0, 1);
            tft_Print("kHz");
        } else {
            tft_print_float(freq, 1);
            tft_Print("Hz");
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
    
    // CH2 Default
    ch2.enabled = 0;
    ch2.focused = 0;
    ch2.volts_div = 1.0;
    ch2.offset = 60; 
    ch2.coupling = COUPL_DC; // Aggiunto accoppiamento di default
    ch2.probe = 0; // Sonda 1X di default
    ch2.inverted = false; // Non invertito di default
    ch2.vdiv_idx = 6; // Indice per 1V/div
    ch2.color = CYAN;
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
                case 21: // Ipotetico tasto fisico "T/Div"
                    currentMenu = MENU_TBASE;
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
                            //cycleVoltDiv(&ch2);
                            break;
                        case 3:  // Tasto 3 
                            cycleProbe(&ch1);
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
                            //cycleVoltDiv(&ch2);
                            break;
                        case 3:  // Tasto 3 
                            cycleProbe(&ch2);
                            break;

                        case 0:  // Tasto 4 
                            toggleInvert(&ch2);
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

                        case 3:  // Tasto 4 -> Attiva Encoder per il LEVEL
                            //toggleTrigLevelMode();
                            //updateSidebarLabels();
                            break;

                        case 0:  // Tasto 5 (Bottom) -> EXIT
                            //currentMenu = MENU_NONE; // O torna al menu precedente
                            //drawStaticInterface();   // Ridisegna tutto per pulire la sidebar
                            
                            updateSidebarLabels();
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

    ch1.vdiv_idx = encoder_values[1]; // Aggiorna Volt/Div CH1
    ch2.vdiv_idx = encoder_values[3]; // Aggiorna Volt/Div

    acquire_and_draw();
    update_status_bar(false);
        

    
    }
}
