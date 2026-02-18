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
//uint8_t buffer_c[BUFFER_TOTAL];
uint16_t old_buffer_a[400];
uint16_t old_buffer_b[400];
//uint8_t old_buffer_c[300];

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
static tdiv_pan_t   mode_tdiv_pan = T_DIV;

uint8_t currentMenu = MENU_CH1; // Default


uint16_t ch1_color = YELLOW; // Il colore del canale 2
uint16_t ch2_color = CYAN; // Il colore del canale 2

bool ch_visible[2] = {true, true}; 
uint8_t* buffers_vecchi[2] = {old_buffer_a, old_buffer_b}; // Puntatori ai tuoi buffer di cancellazione

uint16_t trigger_level_12bit = 0x07FF;
uint16_t y_offset_ch[2] = {120, 120};

//uint8_t ch1_coupling = COUPL_DC; // Stato iniziale
//uint8_t ch2_coupling = COUPL_DC; // Stato iniziale
// Array per memorizzare il fattore di attenuazione dei canali
uint8_t ch_coupling[2] = {COUPL_DC, COUPL_DC};
// 0 = 1X, 1 = 10X, 2 = 100X
uint8_t ch_probe[2] = {0, 0};
// Variabili globali per il calcolo delle tensioni
float ch_multiplier[2] = {1.0, 1.0};
bool ch_inverted[2] = {false, false};
uint8_t encoderMode = MODE_NONE;
int16_t last_trig_y = -1; // Per cancellare la vecchia linea

uint8_t current_time_base_idx = 0;
static bool is_running= true;


const char* v_div_labels[] = {
    "10mV", "20mV", "50mV", 
    "100mV", "200mV", "500mV", 
    "1V", "2V", "5V","10V"
};

#define MAX_VDIV_IDX 9
uint8_t ch1_vdiv_idx = 6; // Default a 1V/div
uint8_t ch2_vdiv_idx = 6; // Default a 1V/div

uint8_t old_ch1_vdiv_idx, old_ch2_vdiv_idx, old_current_time_base_idx = 0xFF;

uint8_t old_ch_coupling[2] = {0xFF, 0xFF} ;
uint16_t old_trigger_level_12bit = 0xFFFF;
uint32_t old_freq = 0xFFFFFFFF;

Point_t old_a = { 0, 0 };
Point_t old_b = { 0, 0 };
Point_t old_c = { 0, 0 };
Point_t gnd_mark_a[2] = {{ 0, 0 }, { 0, 0 }};
Point_t gnd_mark_b[2] = {{ 0, 0 }, { 0, 0 }};
Point_t gnd_mark_c[2] = {{ 0, 0 }, { 0, 0 }};
uint16_t old_y_offset_ch[2];

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

    int16_t offset = (TRACE_W / 2) - view_offset + MARGIN_X;

    Point_t a = { offset - 5, MARGIN_Y };
    Point_t b = { offset + 5, MARGIN_Y };
    Point_t c = { offset, MARGIN_Y + 15 };
    
    if(pan_flag){
        tft_FillTriangle(old_a, old_b, old_c, BLACK);
        old_a = a;
        old_b = b;
        old_c = c;
    }
    tft_FillTriangle(a, b, c, WHITE);
}


void draw_ground_marker(uint8_t channel_idx, uint16_t color) {
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
}

void acquire_and_draw2(){
    // Se siamo in STOP manuale e non stiamo facendo un SINGLE che ha appena finito
    if (!is_running && (trigger_mode != TRIG_MODE_SINGLE || freeze)) {
        return; 
    }
    tft_drawGrid(LIGHTGREY);
    osc_read_triggered(buffer_a, buffer_b);
    //if (ch_visible[0]) {
        draw_trace(buffer_a, old_buffer_a, 400, y_offset_ch[0], GREEN, ch_inverted[0], ch_visible[0], true);
   // }
    
    // Disegna CH2 solo se è visibile
    //if (ch_visible[1]) {
        draw_trace(buffer_b, old_buffer_b, 400, y_offset_ch[1], RED, ch_inverted[1], ch_visible[1], true);
    //}
    draw_trigger_line(trigger_level_12bit, YELLOW, false);
    draw_ground_marker(0, GREEN);
    draw_ground_marker(1, RED);
    //draw_trace(buffer_a, old_buffer_a, 400, CH0_Y, GREEN);
    //draw_trace(buffer_b, old_buffer_b, 400, CH0_Y, RED);
    
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
    draw_trace(buffer_a, old_buffer_a, 400, y_offset_ch[0], GREEN, ch_inverted[0], ch_visible[0], true);
    
    // Disegna CH2
    draw_trace(buffer_b, old_buffer_b, 400, y_offset_ch[1], RED, ch_inverted[1], ch_visible[1], true);
    
    // UI e Marker (Sempre visibili per poterli muovere in STOP)
    draw_trigger_line(trigger_level_12bit, YELLOW, false);
    draw_ground_marker(0, GREEN);
    draw_ground_marker(1, RED);
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
    tft_printAt("RUN", 10, 5, GREEN, DARKGREY);
    //tft_printAt("T: 100uS", 120, 5, WHITE, DARKGREY);
    tft_printAt("Vpp: 3.24V", 250, 5, YELLOW, DARKGREY);

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
    tft_drawLine(SIDEBAR_X - 2, 20, SIDEBAR_X - 2, 320, GREY);

    // 5. Disegno dei 5 Soft-Keys (Tutti BIANCHI come richiesto)
    if (currentMenu == MENU_CH1 || currentMenu == MENU_CH2) {
        uint8_t idx = (currentMenu == MENU_CH1) ? 0 : 1;
        
        // Etichette dinamiche per accoppiamento e sonda
        const char* coupLbl = (ch_coupling[idx] == COUPL_DC) ? "DC" : (ch_coupling[idx] == COUPL_AC ? "AC" : "GND");
        const char* probLbl = (ch_probe[idx] == 0) ? "1X" : (ch_probe[idx] == 1 ? "10X" : "100X");

        drawMenuButton(0, ch_visible[idx] ? (idx==0?"CH1 ON":"CH2 ON") : (idx==0?"CH1 OFF":"CH2 OFF"), true, WHITE);
        drawMenuButton(1, coupLbl, false, WHITE);
        drawMenuButton(2, probLbl, false, WHITE);
        drawMenuButton(3, ch_inverted[idx] ? "-INV-" : "INVERT", false, WHITE);
        drawMenuButton(4, (encoderMode == MODE_Y_POS) ? "> POS <" : "POSITION", false, WHITE);
    }
    else if (currentMenu == MENU_TRIG) {
        drawMenuButton(0, (trigger_mode == 0) ? "AUTO" : "NORMAL", true, WHITE);
        drawMenuButton(1, trigger_slope ? "RISE" : "FALL", false, WHITE);
        drawMenuButton(2, (trigger_source == 1) ? "CH1" : "CH2", false, WHITE);
        drawMenuButton(3, (encoderMode == MODE_TRIG_LEVEL) ? "> LEV <" : "LEVEL", false, WHITE);
        drawMenuButton(4, "BACK", false, WHITE);
    }

    // 6. Ripristina la griglia
    tft_drawGrid(LIGHTGREY);
}

void toggleCH(uint8_t ch) 
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
}


void cycleCoupling(uint8_t ch) 
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
}


void aggiornaMoltiplicatoreSonda(uint8_t ch, uint8_t probe_idx) 
{
    uint8_t idx = ch - 1;

    // Impostiamo il moltiplicatore in base all'indice (0=1X, 1=10X, 2=100X)
    switch(probe_idx) {
        case 0: // 1X
            ch_multiplier[idx] = 1.0;
            break;
        case 1: // 10X
            ch_multiplier[idx] = 10.0;
            break;
        case 2: // 100X
            ch_multiplier[idx] = 100.0;
            break;
    }
}

void cycleProbe(uint8_t ch) 
{
    // Indice array (0 per CH1, 1 per CH2)
    uint8_t idx = ch - 1; 

    // 1. Cicla tra le tre impostazioni
    ch_probe[idx]++;
    if (ch_probe[idx] > 2) {
        ch_probe[idx] = 0;
    }

    // 2. Logica di calcolo
    aggiornaMoltiplicatoreSonda(ch, ch_probe[idx]);

    // 3. Preparazione etichetta
    const char* label;
    switch(ch_probe[idx]) {
        case 0:  label = "1X  ";   break;
        case 1:  label = "10X ";  break;
        case 2:  label = "100X"; break;
        default: label = "??";   break;
    }
    
    // 4. Aggiornamento grafico con il COLORE corretto
    // Determiniamo il colore in base al canale (ch 1 = YELLOW, ch 2 = CYAN)
    //uint16_t color = (ch == 1) ? YELLOW : CYAN;
    uint16_t color = WHITE;
    // Il tasto fisico 6 (ev 6) corrisponde al terzo bottone (indice 2)
    drawMenuButton(2, label, true, color); 
}

float calcolaVoltReali(uint8_t ch, uint8_t valoreADC) {
    uint8_t idx = ch - 1;
    
    // 1. Converti il valore ADC (0-255) in tensione basandoti sul tuo riferimento (es. 5V)
    float v_letta = (valoreADC * 5.0) / 255.0;
    
    // 2. Applica il moltiplicatore della sonda
    return v_letta * ch_multiplier[idx];
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
            
        case MENU_TBASE:
            menuTitle = "T-BASE";
            menuColor = WHITE;
            break;
        
        case MENU_PAN:
            menuTitle = " PAN  ";
            menuColor = MAGENTA;
            break;
            
        default:
            menuTitle = " MENU ";
            menuColor = CYAN;
            break;
    }
    
    // Scriviamo il titolo a destra (X=410) sopra i tasti
    tft_printAt(menuTitle, 425, 5, menuColor, DARKGREY);

    // --- 2. LOGICA TASTI SIDEBAR ---
    if (currentMenu == MENU_CH1 || currentMenu == MENU_CH2) {
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
        drawMenuButton(4, (encoderMode == MODE_Y_POS) ? "> POS <" : "POSITION", (encoderMode == MODE_Y_POS), WHITE);
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
        drawMenuButton(3, (encoderMode == MODE_TRIG_LEVEL) ? "> LEV <" : "LEVEL", (encoderMode == MODE_TRIG_LEVEL), WHITE);

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

void toggleInvert(uint8_t ch) 
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
}

void toggleYPosMode(uint8_t ch) {
    // Se era già attivo, lo spegniamo (toggle), altrimenti lo attiviamo
    if (encoderMode == MODE_Y_POS) {
        encoderMode = MODE_NONE;
    } else {
        encoderMode = MODE_Y_POS;
    }

    // Aggiorniamo la grafica per far capire all'utente che l'encoder è attivo
    //uint16_t color = (ch == 1) ? YELLOW : CYAN;
    uint16_t color = WHITE;
    const char* label = (encoderMode == MODE_Y_POS) ? "> POS <" : "POS Y";
    
    // Il tasto 0 (ev 0) è il quinto bottone (indice 4)
    drawMenuButton(4, label, (encoderMode == MODE_Y_POS), color);
}

void toggleTrigLevelMode() {
    // Se era già attivo, lo spegniamo (toggle), altrimenti lo attiviamo
    if (encoderMode == MODE_TRIG_LEVEL) {
        encoderMode = MODE_NONE;
    } else {
        encoderMode = MODE_TRIG_LEVEL;
    }

    // Aggiorniamo la grafica per far capire all'utente che l'encoder è attivo
    //uint16_t color = (ch == 1) ? YELLOW : CYAN;
    uint16_t color = WHITE;
    const char* label = (encoderMode == MODE_TRIG_LEVEL) ? "> LEVEL <" : "LEVEL";
    
    // Il tasto 0 (ev 0) è il quinto bottone (indice 4)
    drawMenuButton(4, label, (encoderMode == MODE_Y_POS), color);
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
    int16_t y_pixel = y_offset_ch[trigger_source - 1] - (int16_t)(sample * scale_factor);

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

    // 2. Applichiamo la STESSA inversione della traccia
    // Se la traccia è invertita (inverted=false nel draw_trace), facciamo 255 - raw_data
    // Supponiamo che 'inverted' qui segua la stessa logica del canale selezionato
    bool ch_inverted = false; // Metti qui la variabile che passi a draw_trace per quel canale
    if (!ch_inverted) {
        raw_data = 255 - raw_data;
    }

    // 3. Calcolo coordinata Y reale (IDENTICO alla draw_trace)
    // Usiamo y_offset_ch che passi alla draw_trace
    int16_t y = (int16_t)(raw_data / 2) + y_offset_ch[trigger_source - 1];

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
        tft_printAt(label, 10, 5, color, DARKGREY);
        last_ui_state = current_state;
    }

    // --- CANALE 1 ---
    if(old_ch1_vdiv_idx != ch1_vdiv_idx || old_ch_coupling[0] != ch_coupling[0] || force){
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
    }

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



// Valori di default per gli encoder (minimo, massimo step, valore iniziale)
#define OFFSET_Y_MIN -50
#define OFFSET_Y_MAX 250
#define OFFSET_Y_STEP 2
#define OFFSET_Y1_C_VAL 120
#define OFFSET_Y2_C_VAL 120

#define VDIVCH_MIN 1
#define VDIVCH_MAX 10
#define VDIVCH_STEP 1
#define VDIVCH_C_VAL 5

#define TDIV_MIN 0
#define TDIV_MAX 16
#define TDIV_STEP 1
#define TDIV_C_VAL 11

#define TRIG_MIN 0
#define TRIG_MAX 4095
#define TRIG_STEP 64
#define TRIG_C_VAL 2048


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

/************************************************************************************/
/*                                 KEY MAP                                          */
/*          ROW0            ROW1        ROW2                                        */
/*      12 context       13 CH1       14 CH2                                        */
/*       9 context       10 TRIG      11 T/Div                                      */
/*       6 context        7 PAN        8 RUN/STOP                                   */
/*       3 context        4            5 SINGLE                                     */
/*       0 context        1            2                                            */
/********************************************************************************** */
// --- main loop ---
void scope_main(void)
{
    uint8_t key, rep;
    uint8_t new_sel;
    uint16_t new_trigger_level;
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
            switch (ev)
            {
                // --- TASTI FISICI DEDICATI (Master) ---
                case 7: // Tasto PAN 
                    if(currentMenu == MENU_PAN) view_offset = 0; //Riporta in centro
                    else{
                        currentMenu = MENU_PAN;
                        encoderMode = MODE_PAN;
                        updateSidebarLabels(); // Ridisegna etichette 
                    }
                    break;
                case 8:
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
                case 5:
                    trigger_mode = TRIG_MODE_SINGLE;
                    set_trigger_mode(trigger_mode, trigger_slope, trigger_source);
                    freeze = false;     // Fondamentale: permette a osc_read_triggered di armare
                    is_running = true;  // Ci assicuriamo che il loop chiami l'acquisizione
                    REG_TRIG = 0x01;    // Armiamo la FSM FPGA
                    //updateSidebarLabels();
                    update_status_bar(true);
                    break;

                case 10: // Ipotetico tasto fisico "Trigger"
                    currentMenu = MENU_TRIG;
                    updateSidebarLabels(); // Ridisegna etichette 
                    break;
                case 11: // Ipotetico tasto fisico "T/Div"
                    currentMenu = MENU_TBASE;
                    encoderMode = MODE_TBASE;
                    updateSidebarLabels(); // Ridisegna etichette 
                    break;
                case 13: // Ipotetico tasto fisico "Vertical CH1"
                    currentMenu = MENU_CH1;
                    updateSidebarLabels(); // Ridisegna etichette 
                    break;
                case 14: // Ipotetico tasto fisico "Vertical CH2"
                    currentMenu = MENU_CH2;
                    updateSidebarLabels(); // Ridisegna etichette 
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
                            toggleCH(1);
                            break;

                        case 9:  // Tasto 2 
                            cycleCoupling(1);
                            break;

                        case 6:  // Tasto 3 
                            cycleProbe(1);
                            break;

                        case 3:  // Tasto 4 
                            toggleInvert(1);
                            break;

                        case 0:  // Tasto 5 (Bottom) -> EXIT
                            toggleYPosMode(1);
                            break;
                    }
                    break;
                case MENU_CH2:
                    switch (ev) {
                        case 12: // Tasto 1 
                            toggleCH(2);
                            break;

                        case 9:  // Tasto 2 
                            cycleCoupling(2);
                            break;

                        case 6:  // Tasto 3 
                            cycleProbe(2);
                            break;

                        case 3:  // Tasto 4 
                            toggleInvert(2);
                            break;

                        case 0:  // Tasto 5 (Bottom) -> EXIT
                            toggleYPosMode(2);
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
                            toggleTrigLevelMode();
                            updateSidebarLabels();
                            break;

                        case 0:  // Tasto 5 (Bottom) -> EXIT
                            //currentMenu = MENU_NONE; // O torna al menu precedente
                            //drawStaticInterface();   // Ridisegna tutto per pulire la sidebar
                            encoderMode = MODE_NONE; 
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

    y_offset_ch[0] = encoder_values[0]; // Aggiorna posizione Y CH1
    y_offset_ch[1] = encoder_values[2]; // Aggiorna posizione Y CH2

    ch1_vdiv_idx = encoder_values[1]; // Aggiorna Volt/Div CH1
    ch2_vdiv_idx = encoder_values[3]; // Aggiorna Volt/Div

    acquire_and_draw();
    update_status_bar(false);
        

    
    }
}
