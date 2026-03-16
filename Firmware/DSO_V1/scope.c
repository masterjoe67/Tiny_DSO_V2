#include <stdint.h>
#include <stdlib.h>
#include <util/delay.h>
#include <stdbool.h>
#include <math.h>
#include "Peripheral/st7798.h"
#include "Peripheral/input.h"
#include "Peripheral/uart.h"
#include "Peripheral/leds.h"
#include "scope.h"
#include "scope_shared.h"
#include "display_manager.h"
#include "fpga_manager.h"
#include "autoset_manager.h"
#include "encoder_manager.h"
#include "trace_manager.h"
#include "trigger_manager.h"
#include "cursor_manager.h"



/* --- MAPPATURA HARDWARE & MEMORIA CONDIVISA --- */
// Puntatore alla struct delle misure (mappata dopo i buffer campioni)
ScopeMeasures *misure = (ScopeMeasures *)0x3000;

// Puntatori ai buffer per la cancellazione della traccia precedente (Double Buffering logico)
int16_t* buffers_vecchi[2] = { ADDR_OLD_A, ADDR_OLD_B };

/* --- STATO DEI CANALI --- */
Channel ch1, ch2;

/* --- SISTEMA DI CONTROLLO & MENU --- */
uint8_t currentMenu = MENU_CH1;         // Menu attualmente attivo
uint8_t oldMenu = 0xFF;                 // Memoria per forzare il refresh del menu al boot
bool is_running = true;                 // Stato acquisizione (Run/Stop)
bool freeze = false;                    // Stato congelamento schermo
bool pan_flag = false;                  // Flag per spostamento orizzontale della traccia

/* --- CONFIGURAZIONE TEMPI (TIMEBASE) --- */
uint8_t current_time_base_idx = 0;      // Indice attuale della base tempi
uint8_t old_current_time_base_idx = 0xFF; // Memoria base tempi per refresh display
uint8_t time_div_sel = 10;              // Selezione divisione temporale
bool time_div_sel_changed = true;       // Flag per ricalcolo parametri temporali
uint8_t prev_time_div_sel = 0xFF;       // Memoria per rilevamento cambio scala

/* --- CONFIGURAZIONE TRIGGER --- */
uint16_t trigger_level_12bit = 0x07FF;  // Livello trigger (Default a metà scala ADC 12-bit)
uint16_t old_trigger_level_12bit = 0xFFFF; // Memoria per aggiornamento grafica trigger
uint8_t  trigger_source = 1;            // Sorgente trigger (1 = CH1, 2 = CH2)
uint8_t  trigger_hysteresis = 0x20;     // Isteresi antirumore (LSB)

trigger_mode_t trigger_mode = TRIG_MODE_AUTO;   // Modalità: AUTO, NORMAL, SINGLE
trig_slope_t   trigger_slope = TRIG_SLOPE_RISING; // Pendenza: SALITA, DISCESA

/* --- MEMORIE DI STATO PER OTTIMIZZAZIONE GRAFICA --- */
// Queste variabili evitano di riscrivere testi o pixel se i valori non cambiano
int16_t prev_view_offset = 0xFFFF;
uint16_t prev_trigger_level = 0xFFFF;
int16_t prev_enc_trig_val = 0xFFFF;
uint8_t old_meas_source = 1;
uint8_t old_meas_type = 1;
uint8_t old_meas_active = 2;
uint8_t old_f_active = 0;

/* --- INPUT & ENCODER --- */
static int16_t last_enc1 = 0;           // Memoria posizione encoder 1
static int16_t last_enc2 = 0;           // Memoria posizione encoder 2

uint8_t is_xy_mode = Y_T;

uint8_t is_vectors = 1;

/* --- TABELLE DI LOOKUP (COSTANTI) --- */

// Valori reali delle divisioni verticali (V/div)
const float v_div_values[] = {
    0.01, 0.02, 0.05,   // 10mV, 20mV, 50mV
    0.1,  0.2,  0.5,    // 100mV, 200mV, 500mV
    1.0,  2.0,  5.0,    10.0 // 1V, 2V, 5V, 10V
};

// Etichette testuali per la base tempi
const char* time_base_labels[] = {
    "250ns", "500ns", "1us",   "2us",   "5us",   "10us",  "20us",  "50us", 
    "100us", "200us", "500us", "1ms",   "2ms",   "5ms", 
    "10ms",  "20ms",  "50ms",  "100ms", "200ms", "500ms", 
    "1s"
};

/** * Step dinamici per il trigger.
 * All'aumentare del Volts/div (chX.volts_div), la sensibilità del trigger 
 * deve diminuire per non essere troppo "nervoso" su scale grandi.
 */
const uint16_t trigger_steps_table[10] = {
    1,   // 0: 10mV
    1,   // 1: 20mV
    3,   // 2: 50mV
    6,   // 3: 100mV
    12,  // 4: 200mV
    32,  // 5: 500mV
    64,  // 6: 1V
    128, // 7: 2V
    320, // 8: 5V
    640  // 9: 10V
};


// Tabella valori DDS aggiornata (21 ingressi)
// Indice 0: 250ns/div ... Indice 20: 1s/div
const uint32_t dds_table[21] PROGMEM = {
    0xFFFFFFFF, // 0: 250ns/div (Zoom x4 - F_samp equiv 160MHz*)
    0xFFFFFFFF, // 1: 500ns/div (Zoom x2 - F_samp equiv 80MHz*)
    0xAAAAAAAA, // 2: 1us/div    (F_samp = 40MHz)
    0x55555555, // 3: 2us/div    (F_samp = 20MHz)
    0x22222222, // 4: 5us/div    (F_samp = 8MHz)
    0x11111111, // 5: 10us/div   (F_samp = 4MHz)
    0x08888888, // 6: 20us/div   (F_samp = 2MHz)
    0x0369D036, // 7: 50us/div   (F_samp = 800kHz)
    0x01B4E81B, // 8: 100us/div  (F_samp = 400kHz)
    0x00DA740D, // 9: 200us/div  (F_samp = 200kHz)
    0x00575C28, // 10: 500us/div (F_samp = 80kHz)
    0x002BEE7D, // 11: 1ms/div   (F_samp = 40kHz)
    0x0015F73E, // 12: 2ms/div   (F_samp = 20kHz)
    0x0008C995, // 13: 5ms/div   (F_samp = 8kHz)
    0x000464CA, // 14: 10ms/div  (F_samp = 4kHz)
    0x00023265, // 15: 20ms/div  (F_samp = 2kHz)
    0x0000E5BC, // 16: 50ms/div  (F_samp = 800Hz)
    0x000072DE, // 17: 100ms/div (F_samp = 400Hz)
    0x0000396F, // 18: 200ms/div (F_samp = 200Hz)
    0x000016F3, // 19: 500ms/div (F_samp = 80Hz)
    0x00000B79  // 20: 1s/div     (F_samp = 40Hz)
};

const float timebase_seconds[] = {
    0.00000025f, // 0: 250ns
    0.00000050f, // 1: 500ns
    0.00000100f, // 2: 1us
    0.00000200f, // 3: 2us
    0.00000500f, // 4: 5us
    0.00001000f, // 5: 10us
    0.00002000f, // 6: 20us
    0.00005000f, // 7: 50us
    0.00010000f, // 8: 100us
    0.00020000f, // 9: 200us
    0.00050000f, // 10: 500us
    0.00100000f, // 11: 1ms
    0.00200000f, // 12: 2ms
    0.00500000f, // 13: 5ms
    0.01000000f, // 14: 10ms
    0.02000000f, // 15: 20ms
    0.05000000f, // 16: 50ms
    0.10000000f, // 17: 100ms
    0.20000000f, // 18: 200ms
    0.50000000f, // 19: 500ms
    1.00000000f  // 20: 1s
};

/***************************************************************************************
** Function name:           acquire_and_draw
** Description:             Coordina l'acquisizione dei campioni dall'FPGA e il rendering
** della traccia a schermo, gestendo il refresh dei buffer.
***************************************************************************************/
void acquire_and_draw(){
    // 1. ACQUISIZIONE (Condizionale)
    // Proviamo a leggere solo se siamo in RUN o in un SINGLE attivo
    //if (is_running || (trigger_mode == TRIG_MODE_SINGLE && !freeze)) {
    if (is_running || (trigger_mode == TRIG_MODE_SINGLE && !freeze) || pan_flag) {
        osc_read_triggered();
    }

    

    if(!is_xy_mode){
        tft_drawGrid(LIGHTGREY);
        draw_dual_trace_from_bram(&ch1, &ch2, old_buffer_a, old_buffer_b, 400, is_vectors);
        draw_trigger_line(trigger_level_12bit, ORANGE, false);
        draw_ground_marker(&ch1);
        draw_ground_marker(&ch2);
        drawPanTrack();
        aggiorna_grafica_cursori();
    }else{
        tft_drawGrid_XY(LIGHTGREY);
        draw_xy_trace(&ch1, &ch2, old_buffer_a, old_buffer_b, 400);
    }
    
    
}

/***************************************************************************************
** Function name:           cycleCoupling
** Description:             Gestisce la commutazione ciclica dell'accoppiamento del canale
** (DC, AC, GND) e aggiorna l'interfaccia utente.
***************************************************************************************/
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

/***************************************************************************************
** Function name:           toggleBWLimit
** Description:             Attiva o disattiva il filtro di limitazione della banda
** passante (Bandwidth Limit) per il canale selezionato.
***************************************************************************************/
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

/***************************************************************************************
** Function name:           aggiornaMoltiplicatoreSonda
** Description:             Cicla il fattore di attenuazione della sonda (1x, 10x, 100x)
** e ricalcola i parametri di visualizzazione per mantenere corretta la scala Volt.
***************************************************************************************/
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

/***************************************************************************************
** Function name:           cycleProbe
** Description:             Cicla le impostazioni di attenuazione della sonda (1X, 10X, 100X)
** aggiornando i parametri di calcolo e l'interfaccia grafica del menu.
***************************************************************************************/
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

/***************************************************************************************
** Function name:           toggleFineCoarse
** Description:             Alterna tra la regolazione a passi standard (Coarse) e la
** regolazione fine dei Volt/div, riconfigurando l'encoder.
***************************************************************************************/
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

/***************************************************************************************
** Function name:           toggleInvert
** Description:             Inverte la polarità del segnale visualizzato per il canale
** selezionato e aggiorna lo stato dell'interfaccia utente.
***************************************************************************************/
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

/***************************************************************************************
** Function name:           handle_channel_button
** Description:             Gestisce la logica di accensione, spegnimento e focus dei
** canali (CH1/CH2). Include la pulizia dei residui grafici alla disattivazione.
***************************************************************************************/
void handle_channel_button(uint8_t channel_num) {
    Channel *current = (channel_num == 1) ? &ch1 : &ch2;
    Channel *other   = (channel_num == 1) ? &ch2 : &ch1;
    uint8_t menu_target = (channel_num == 1) ? MENU_CH1 : MENU_CH2;
    uint8_t idx = channel_num - 1;

    // CASO A: Canale spento -> Accendi e vai al suo menu
    if (!current->enabled) {
        current->enabled = 1;
        current->focused = 1;
        other->focused = 0;
        currentMenu = menu_target;
    } 
    // CASO B: Acceso ma non ha il focus OPPURE ha il focus ma il menu è un altro (es. CURSOR)
    else if (!current->focused || currentMenu != menu_target) {
        current->focused = 1;
        other->focused = 0;
        currentMenu = menu_target;
    } 
    // CASO C: Già acceso, già focused e siamo già nel suo menu -> Spegni il canale
    else {
        current->enabled = 0;
        current->focused = 0;
        
        // Pulizia tracce "fantasmi"
        for (uint16_t i = 0; i < 400; i++) {
            tft_drawPixel(i + MARGIN_X, buffers_vecchi[idx][i], BLACK);
        }
        
        // Opzionale: se spegni il canale, torna al menu principale
        currentMenu = MENU_NONE;
    }
    
    updateSidebarLabels();
}

/***************************************************************************************
** Function name:           init_channels
** Description:             Inizializza le strutture dati di entrambi i canali con i
** parametri di default (scala, offset, accoppiamento e colori).
***************************************************************************************/
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

/***************************************************************************************
** Function name:           updateChannelVoltDiv
** Description:             Aggiorna la scala di ampiezza (Volt/div) del canale in modalità
** Coarse o Fine e adegua la sensibilità dell'encoder del trigger.
***************************************************************************************/
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


        if(trigger_source == 1 && ch == &ch1) {
            configure_encoder(5, PARAM_STEP, calcola_step_trigger(ch->volts_div)); // Aggiorna dinamicamente lo step del trigger
        }
        if(trigger_source == 2 && ch == &ch2) {
            configure_encoder(5, PARAM_STEP, calcola_step_trigger(ch->volts_div)); // Aggiorna dinamicamente lo step del trigger
        }
    }
}

/***************************************************************************************
** Function name:           set_display_mode
** Description:             Cambia la modalità di visualizzazione (YT <-> XY).
** Gestisce la transizione di stato resettando i buffer di 
** cancellazione per evitare "fantasmi" grafici e riconfigura 
** l'interfaccia per riflettere i nuovi assi.
** Parameters:              mode: 0 per Y-T (Standard), 1 per X-Y
***************************************************************************************/
void set_display_mode(uint8_t mode) {
    is_xy_mode = mode;

    // Puliamo l'area traccia per evitare sovrapposizioni sporche
    tft_fillRect(MARGIN_X, MARGIN_Y, TRACE_W, TRACE_H, BLACK);

    // --- SETUP SPECIFICO ---
    if (is_xy_mode) {
        // --- SE ENTRIAMO IN MODALITÀ X-Y ---
        ch1.enabled = true;
        ch2.enabled = true;
        


    } else {
        // --- SE TORNIAMO IN MODALITÀ Y-T (Standard) ---

    }

    // --- AGGIORNAMENTO INTERFACCIA ---
    updateSidebarLabels();
}

/***************************************************************************************
** Function name:           scope_main
** Description:             Ciclo principale (Super-Loop) dell'applicativo. Gestisce la
** schedulazione delle attività di acquisizione, polling degli
** input e aggiornamento periodico della logica dei cursori.
***************************************************************************************/
void scope_main(void)
{
    uint8_t key, rep;
    uint8_t new_sel;
    // Piccolo check dei LED (POST - Power On Self Test)
LED_SetCH(1, 1);
_delay_ms(100);
LED_SetCH(2, 1);
_delay_ms(100);
LED_SetRunStop(LED_YELLOW);

    drawStaticInterface();
    init_timer_polling();
    init_channels();
    conf_encoder();
    
   
    set_base_time(19);

    set_trigger_level(trigger_level_12bit);   
    set_trigger_mode(TRIG_MODE_AUTO, TRIG_SLOPE_RISING, trigger_source);
    
    misure->source = 1; // Default su CH1
    misure->type = 0;   // Default su Vpp
    misure->active = 0;
    misure->f_active = 0;

    aggiorna_parametri_hw(&ch1);
    aggiorna_parametri_hw(&ch2);

    scope_set_hysteresis(20); // Imposta un valore di default per l'isteresi
    update_status_bar(true);
    _delay_ms(500);
    LED_SetCH(1, 0);
    LED_SetCH(2, 0);
    LED_SetRunStop(LED_GREEN); 

   while(1)
    {
        pan_flag = false;
        uint8_t ev = 0xff;
        if (keypad_poll(&key, &rep)) {
            ev = key;

        } else {
            ev = 0xFF;
        }
        
        
        if(ev != 0xFF){
 
            switch (ev)
            {
                // --- TASTI FISICI DEDICATI (Master) ---
                case KEY_AUTOSET: // Tasto AUTOSET
                    currentMenu = MENU_NONE; // Esci da eventuali menu per avere un'interfaccia pulita durante l'autoset
                    cursor_type = CUR_OFF; // Disattiva eventuali cursori attivi
                     routine_autoset_dual();
                     updateSidebarLabels(); // Aggiorna le label dopo l'autoset
                    
                    break;
                case KEY_CURSORS: // Tasto fisico "Cursors"
                    if(currentMenu != MENU_CURSORS) {
                        currentMenu = MENU_CURSORS;
                        updateSidebarLabels(); 
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
                case KEY_MEASURE: // Ipotetico tasto fisico "Measure"
                    currentMenu = MENU_MEAS;
                    updateSidebarLabels(); 
                    break;
                case KEY_CH1: // Tasto fisico "Vertical CH1"
                    handle_channel_button(1);
                    break;
                case KEY_CH2: // Tasto fisico "Vertical CH2"
                    handle_channel_button(2);
                    break;
                case KEY_ORIZZONTAL:
                    currentMenu = MENU_ORIZZONTAL;
                    updateSidebarLabels(); 
                    break;
                case 15: // Tasto encoder per la posizione verticale (Y-POS)
                    write_encoder(0, OFFSET_Y1_C_VAL); // Reset posizione Y CH1 
                    break;
                case 16: // Tasto encoder per la posizione verticale (Y-POS)
                    write_encoder(2, OFFSET_Y2_C_VAL); // Reset posizione Y CH2
                    break;
                case 17: // Tasto encoder per il livello di trigger
                  
                    if (current_enc_mode == ENC_MODE_HYSTERESIS) {
                        // RESET ISTERESI al valore di default (es. 20)
                        trigger_hysteresis = 20; 
                        scope_set_hysteresis(trigger_hysteresis);
                        
                        // Sincronizziamo il valore interno dell'encoder
                        configure_encoder(5, PARAM_C_VAL, trigger_hysteresis); 
                        
                    } 
                    else {
                        write_encoder(5, TRIG_C_VAL);
                    }
                    break;
                case 18: // Tasto encoder per il PAN
                    write_encoder(6, 0); 
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
                            if (current_enc_mode == ENC_MODE_TRIGGER_LEVEL) {
                                current_enc_mode = ENC_MODE_HYSTERESIS;
                                // Opzionale: pre-carica l'encoder con il valore attuale dell'isteresi
                                configure_encoder(5, PARAM_MIN, 0);
                                configure_encoder(5, PARAM_MAX, 255);
                                configure_encoder(5, PARAM_STEP, 1);
                                configure_encoder(5, PARAM_C_VAL, trigger_hysteresis);
                            } else {
                                set_trig_enc_default();
                            }
                            updateSidebarLabels();
                            break;

                        case 0:  // Tasto 5 
                            //currentMenu = MENU_NONE; // O torna al menu precedente
                            //drawStaticInterface();   // Ridisegna tutto per pulire la sidebar
                            
                            //updateSidebarLabels();
                            break;
                    }
                    break;
                case MENU_CURSORS:
                    switch (ev) {
                        case 12: 
                            cursor_type++;
                            if (cursor_type > CUR_TIME) {
                                cursor_type = CUR_OFF;
                                LED_SetCH(1, 0);
                                LED_SetCH(2, 0);
                            } else {
                                LED_SetCH(1, 1);
                                LED_SetCH(2, 1);
                            } 
                            if (cursor_type == CUR_VOLT) {
                                // Configura Encoder 1 per Cursore A (Verticale: 0-240)
                                setup_encoder(0, cursor_v_a, MARGIN_Y, MARGIN_Y + 239, 1); 
                                // Configura Encoder 2 per Cursore B (Verticale: 0-240)
                                setup_encoder(2, cursor_v_b, MARGIN_Y, MARGIN_Y + 239, 1);
                            } else {
                                // Configura per Cursore Tempo (Orizzontale: 0-400)
                                setup_encoder(0, cursor_h_a, MARGIN_X, MARGIN_X + 400, 1);
                                setup_encoder(2, cursor_h_b, MARGIN_X, MARGIN_X + 400, 1);
                            }
                            updateSidebarLabels(); 

                            break;

                        case 9:  // Tasto 2 
                            cursor_source++;
                            if (cursor_source > 2) cursor_source = 1;
                            updateSidebarLabels();
                            break;
                        case 6:  // Tasto 3 
                            
                            break;
                        case 3:  // Tasto 4 
                            
                            break;

                        case 0:  // Tasto 5 
                            
                            break;
                    }
                    break;
                case MENU_ORIZZONTAL:
                    switch (ev) {
                        case 9: 
                            is_vectors++;
                            if (is_vectors > 1) {
                                is_vectors = 0;
                            }
                            updateSidebarLabels(); 

                            break;

                        case 12:  // Tasto 2 
                            set_display_mode(!is_xy_mode); // Inverte lo stato (0->1 o 1->0)
                            updateSidebarLabels();
                            break;
                        case 6:  // Tasto 3 
                            
                            break;
                        case 3:  // Tasto 4 
                            
                            break;

                        case 0:  // Tasto 5 
                            
                            break;
                    }
                    break;

                
            }

           
        }

    update_all_encoders();

    new_sel = encoder_values[4];
    if (new_sel != prev_time_div_sel) {
        // Aggiorna il registro solo se il valore è cambiato
        //REG_BASETIME = new_sel;
        set_base_time(new_sel);
        prev_time_div_sel = new_sel;
        time_div_sel = new_sel; // aggiorna il valore corrente
        time_div_sel_changed = true;
        current_time_base_idx =time_div_sel;
        freeze = false;     // Fondamentale: permette a osc_read_triggered di armare
        is_running = true;  // Ci assicuriamo che il loop chiami l'acquisizione
        REG_TRIG = 0x01;    // Armiamo la FSM FPGA
        updateSidebarLabels(); 
    }
   
    int16_t new_val = encoder_values[5];

if (new_val != prev_enc_trig_val) {
    if (current_enc_mode == ENC_MODE_HYSTERESIS) {
        // --- MODALITÀ ISTERESI ---
        // Limitiamo l'isteresi tra 1 e 255 per sicurezza
        if (new_val < 1) new_val = 1;
        if (new_val > 255) new_val = 255;
        
        trigger_hysteresis = (uint8_t)new_val;
        scope_set_hysteresis(trigger_hysteresis);
        updateSidebarLabels(); // Aggiorna i testi a video (Livello o Hyst)
    } else {
        // --- MODALITÀ LIVELLO TRIGGER (Normale) ---
        trigger_level_12bit = new_val;
        set_trigger_level(trigger_level_12bit);
    }

    prev_enc_trig_val = new_val;
}

    int16_t new_pan = encoder_values[6];
    if (new_pan != prev_view_offset) {
        
        // Aggiorna il registro solo se il valore è cambiato
        osc_write_view_offset(new_pan);
        osc_arm_readout(); 
        prev_view_offset = new_pan;
        view_offset = new_pan; // aggiorna il valore corrente
        pan_flag = true;
    }



    if (currentMenu == MENU_CURSORS) {
        if (cursor_type == CUR_VOLT) {
            cursor_v_a = encoder_values[0]; // Aggiorna posizione cursore verticale A
            cursor_v_b = encoder_values[2]; // Aggiorna posizione cursore verticale B
        } else if (cursor_type == CUR_TIME) {
            cursor_h_a = encoder_values[0]; // Aggiorna posizione cursore orizzontale A
            cursor_h_b = encoder_values[2]; // Aggiorna posizione cursore orizzontale B
        }
    } else {
        // Logica normale: Encoder 1 -> CH1 Offset, Encoder 2 -> CH2 Offset
        ch1.offset = encoder_values[0];
        ch2.offset = encoder_values[2];
    }


    // Aggiorna Canale 1 (usa encoder_values[1])
    updateChannelVoltDiv(&ch1, encoder_values[1], &last_enc1);

    // Aggiorna Canale 2 (usa encoder_values[2] - o quello che hai assegnato)
    updateChannelVoltDiv(&ch2, encoder_values[3], &last_enc2);

   

    acquire_and_draw();
    update_status_bar(false);
    aggiorna_parametri_hw(&ch1);
    aggiorna_parametri_hw(&ch2);
    
    }
}
