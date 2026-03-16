#include <stdint.h>
#include <stdlib.h>

#include "display_manager.h"
#include "scope_shared.h"
#include "fpga_manager.h"
#include "trace_manager.h"
#include "cursor_manager.h"
#include "encoder_manager.h"
#include "Peripheral/st7798.h" 
#include "Peripheral/input.h"
#include "Peripheral/leds.h"

float old_freq = 0xFFFFFFFF;

// Accoda una stringa a un'altra e restituisce il puntatore alla fine
// Molto utile per concatenare più pezzi in sequenza
char* append_str(char* dest, const char* src) {
    while (*src) {
        *dest++ = *src++;
    }
    *dest = '\0'; // Chiude sempre la stringa
    return dest;  // Ritorna la nuova posizione del cursore
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

void drawTextButton(uint8_t index, const char* data1, const char* data2, uint16_t color) {
    uint16_t y = 25 + (index * 49); // Calcola posizione Y in base all'indice
    uint16_t bgColor = BLACK;       // Definiamo lo sfondo fisso a nero
    
    tft_fillRect(410, y + 12, 68, 34, bgColor); // Pulisce l'area del bottone prima di ridisegnarlo
    // 1. Disegna la cornice del bottone
    //tft_drawRect(410, y, 65, 40, color);
    tft_drawFastHLine(415, y, 58, color); // Linea di divisione orizzontale

    // 3. Scrivi il testo passando tutti i parametri richiesti dalla tua funzione
    tft_printCenteredX(data1, 410, 479, y + 15, color, bgColor, 2); 
    tft_printCenteredX(data2, 410, 479, y + 32, color, bgColor, 2);
}

void update_status_bar(bool force) {
    uint16_t yPos = MARGIN_Y + TRACE_H + 10;
    uint16_t xStart = MARGIN_X;
    setTextSize(1);
    tft_printAt("Mje", 10, 5, GREEN, DARKGREY, 2);
    static ui_status_t last_ui_state = 0xFF; // Valore impossibile per forzare il primo disegno
    ui_status_t current_state = get_system_status_code();
    if (force || current_state != last_ui_state) {
        // Solo quando lo stato CAMBIA davvero, facciamo il lavoro pesante
        char* label;
        uint16_t color;

        switch (current_state) {
    case UI_STATUS_STOP:
        label = "STOP  ";
        color = RED;
        LED_SetRunStop(LED_RED);
        break;

    case UI_STATUS_WAIT:
        label = "WAIT  ";
        color = YELLOW;
        LED_SetRunStop(LED_YELLOW);
        break;

    case UI_STATUS_TRIGD:
        label = "TRIG'D";
        color = GREEN;
        LED_SetRunStop(LED_GREEN);
        break;

    case UI_STATUS_RUN:
        label = "RUN   ";
        color = GREEN;
        LED_SetRunStop(LED_GREEN);
        break;

    default:
        label = "???";
        color = WHITE;
        LED_SetRunStop(LED_OFF); // Meglio spegnere se lo stato è ignoto
        break;
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


   if(old_trigger_level_12bit != trigger_level_12bit || force){
    tft_fillRect(xStart + 330, yPos, 100, 16, BLACK);
    setTextColor(GREEN, BLACK);
    tft_set_cursor(xStart + 330, yPos);
    tft_Print("Trig: ");

    // 1. Calcolo del livello relativo allo zero dell'ADC (es. 2048)
    // Sostituisci 'ch1.zero_adc' con la variabile che usi per lo zero del canale sorgente
    int32_t relative_level = 2048 -(int32_t)trigger_level_12bit; 

    // 2. Calcolo in millivolt usando il riferimento reale a 5V (5000mV)
    // Usiamo uint32_t per evitare overflow durante la moltiplicazione
    int32_t level_mv = (relative_level * 5000L) / 4096;

    // 3. Stampa con segno (per vedere +0.50V o -0.20V)
    if(level_mv >= 0) tft_Print("+");
    // Se level_mv è negativo, tft_print_float di solito gestisce già il segno meno
    
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
        tft_fillRect(MARGIN_X + 230, yPos + 20, 120, 16, BLACK);
        // Pulizia locale prima di scrivere il nuovo valore (opzionale se tft_Print sovrascrive bene)
        // tft_fillRect(MARGIN_X + 230, yPos + 20, 80, 10, BLACK); 

        tft_Print("F:");
        if (freq > 1000000) {
            tft_print_float(freq / 1000000.0, 2);
            tft_Print(" MHz");
        } else if (freq > 1000) {
            tft_print_float(freq / 1000.0, 1);
            tft_Print(" kHz");
        } else {
            tft_print_float(freq, 1);
            tft_Print(" Hz "); // Spazio finale per pulire eventuali residui di cifre precedenti
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

void drawStaticInterface() {
    // 1. Pulisce tutto lo schermo
    tft_fillScreen(BLACK);
    
    // 2. Barra Superiore (Status e Misure rapide)
    tft_fillRect(0, 0, 409, 20, DARKGREY);
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

void drawMenuButton(uint8_t index, const char* label, const char* data, bool active, uint16_t color) {
    uint16_t y = 25 + (index * 49); // Calcola posizione Y in base all'indice
    uint16_t bgColor = BLACK;       // Definiamo lo sfondo fisso a nero
    
    tft_fillRect(410, y, 68, 48, bgColor); // Pulisce l'area del bottone prima di ridisegnarlo
    // 1. Disegna la cornice del bottone
    //tft_drawRect(410, y, 65, 40, color);
    tft_drawFastHLine(415, y, 58, color); // Linea di divisione orizzontale

    // 3. Scrivi il testo passando tutti i parametri richiesti dalla tua funzione
    tft_printCenteredX(label, 410, 479, y + 5, color, bgColor, 1); 
    tft_printCenteredX(data, 410, 479, y + 20, color, bgColor, 2);
}

void updateSidebarLabels() {
    if(currentMenu != oldMenu) {
        set_trig_enc_default();
        cursor_type = CUR_OFF; // Spegniamo i cursori quando cambiamo menu per evitare confusione

        setup_encoder(0, OFFSET_Y1_C_VAL, OFFSET_Y_MIN, OFFSET_Y_MAX, OFFSET_Y_STEP);
        setup_encoder(2, OFFSET_Y2_C_VAL, OFFSET_Y_MIN, OFFSET_Y_MAX, OFFSET_Y_STEP);


        oldMenu = currentMenu;
    }
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
        
        case MENU_CURSORS:
            menuTitle = "CURSORS";
            menuColor = GREENYELLOW;
            break;

        case MENU_ORIZZONTAL:
            menuTitle = "ORIZZ.";
            menuColor = PINK;
            break;
            
        default:
            menuTitle = " MENU ";
            menuColor = CYAN;
            break;
    }
    
    // Scriviamo il titolo del menu centrato sopra i tasti, con il colore specifico
    tft_fillRect(410, 0, 79, 20, BLACK);
    //tft_fillRect(411, 20, 68, 48, BLACK); // Pulisce l'area dei tasti per evitare residui di menu precedenti
    tft_printCenteredX(menuTitle, 410, 475, 5, menuColor, BLACK, 2); // Opzione centrata

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
        const char* vdivLabel[] = {"COARSE", "Fine"};
        drawMenuButton(2, "V/DIV", vdivLabel[ch->isFine], false, WHITE);

        // TASTO 3: Sonda (Aggiungi 'probe' alla struct!)
        const char* probeLabels[] = {"1X", "10X", "100X"};
        drawMenuButton(3, "Probe", probeLabels[ch->probe], true, WHITE);

        // TASTO 3: Inversione (Usiamo ch->inverted)
        drawMenuButton(4, "Invert", ch->inverted ? "ON" : "OFF", ch->inverted, WHITE);

    }
    
    else if (currentMenu == MENU_TRIG) {
    // TASTO 0: Sorgente (CH1/CH2)
    drawMenuButton(0, "Source", (trigger_source == 1) ? "CH1" : "CH2", true, WHITE);

    // TASTO 1: Slope (RISE/FALL)
    drawMenuButton(1, "Slope", (trigger_slope == 1) ? "RISE" : "FALL", true, WHITE);

    // TASTO 2: Modalità (AUTO/NORMAL)
    drawMenuButton(2, "Mode", (trigger_mode == 0) ? "AUTO" : "NORM", true, WHITE);

    // TASTO 3: Funzione Encoder (Level / Hysteresis)
    // Se l'encoder è in modalità Hysteresis, evidenziamo il tasto o cambiamo testo
    char hyst_str[8];
    utoa((unsigned int)trigger_hysteresis, hyst_str, 10);
    if (current_enc_mode == ENC_MODE_HYSTERESIS) {
        
        drawMenuButton(3, "Hyst:", hyst_str, true, YELLOW); // Cambia colore per attirare l'attenzione
    } else {
        drawMenuButton(3, "Hyst:", hyst_str, true, WHITE);
    }

    // TASTO 4: Libero (o magari un Reset Trigger)
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
    else if (currentMenu == MENU_CURSORS) {
        // TASTO 0: Tipo di Cursore (OFF, VOLT, TIME)
        const char* typeLabels[] = {"OFF", "VOLT", "TIME"};
        drawMenuButton(0, "Type", typeLabels[cursor_type], (cursor_type > 0), WHITE);
        drawMenuButton(1, "Source", (cursor_source == 1) ? "CH1" : "CH2", true, WHITE);
        // TASTO 1: Sorgente (CH1 / CH2)
        if(cursor_type == CUR_OFF){
            
            drawMenuButton(2, "", "", true, WHITE);
            drawMenuButton(3, "", "", true, WHITE);
            drawMenuButton(4, "", "", true, WHITE);
        }else if(cursor_type == CUR_VOLT){

            drawMenuButton(2, "dV", "", false, WHITE);
            drawMenuButton(3, "Cursor 1", "", false, WHITE);
            drawMenuButton(4, "Cursor 2", "", false, WHITE);
        }else if (cursor_type == CUR_TIME){

            drawMenuButton(2, "dT dF", "", false, WHITE);
            drawMenuButton(3, "Cursor 1", "", false, WHITE);
            drawMenuButton(4, "Cursor 2", "", false, WHITE);
        }
        
    }
    else if (currentMenu == MENU_ORIZZONTAL) {
        // TASTO 0: Tipo di Cursore (OFF, VOLT, TIME)
        
        drawMenuButton(0, "Mode", is_xy_mode ? "X-Y" : "Y-T", true, WHITE);
        drawMenuButton(1, "Trace", is_vectors ? "Vectors" : "Points", true, WHITE);
        drawMenuButton(2, "", "", true, WHITE);
        drawMenuButton(3, "", "", true, WHITE);
        drawMenuButton(4, "", "", true, WHITE);
        
    }

    tft_drawFastHLine(415, TRACE_H + MARGIN_Y + 2, 58, WHITE);
    tft_drawFastHLine(415, TRACE_H + MARGIN_Y + TRACE_H + 2, 58, WHITE);
}