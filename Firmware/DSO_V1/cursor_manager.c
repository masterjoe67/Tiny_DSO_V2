#include <stdint.h>
#include <stdlib.h>
#include "cursor_manager.h"
#include "scope_shared.h"
#include "display_manager.h" // Per drawTextButton e append_str

// =======================================================================================
// SEZIONE: GESTIONE CURSORI (LOGICA E GRAFICA)
// =======================================================================================

// --- Definizioni Colori ---
#define CURSOR_COLOR PINK  // Colore distintivo per i cursori sullo schermo

// --- Variabili di Stato Cursori ---
CursorType cursor_type = CUR_OFF;   // Tipo di cursore attivo (OFF, VOLT, TIME)
uint8_t cursor_source  = 1;         // Sorgente di misura: 1 = CH1, 2 = CH2
uint8_t cursor_select  = 0;         // Focus dell'encoder: 0 = Cursore A, 1 = B, 2 = Entrambi (Link)

// --- Posizioni in Pixel ---
// Cursori di Tensione (Orizzontali, si muovono sull'asse Y: 0-240)
int16_t cursor_v_a = 50;            
int16_t cursor_v_b = 150;

// Cursori di Tempo (Verticali, si muovono sull'asse X: 0-400)
int16_t cursor_h_a = 100;           
int16_t cursor_h_b = 300;

// --- Buffer di Testo ---
// Stringhe per visualizzare i tempi assoluti di T(A) e T(B) nell'interfaccia
char str_ta[20];
char str_tb[20];

/***************************************************************************************
** Function name:           format_val_unit
** Description:             Formatta un valore float in una stringa leggibile, gestendo
** automaticamente i prefissi (n, u, m per i segnali piccoli
** o K, M per le alte frequenze) e l'unità di misura.
*************************************************************************************/
void format_val_unit(char* dest, float val, uint8_t mode, const char* unit) {
    char n_buf[24];
    float abs_v = (val < 0) ? -val : val;
    char* p = dest;

    if (mode == 0) { // Volt o Tempo
        if (val < 0) *p++ = '-';
        else if (val > 0) *p++ = '+';

        if (abs_v == 0) {
            p = append_str(p, "0.00");
        } else if (abs_v < 1e-6f) { // nano
            dtostrf(abs_v * 1e9f, 1, 0, n_buf);
            p = append_str(p, n_buf); p = append_str(p, "n");
        } else if (abs_v < 0.0009995f) { // micro (soglia leggermente sotto 1ms per arrotondamento)
            dtostrf(abs_v * 1e6f, 1, 2, n_buf);
            p = append_str(p, n_buf); p = append_str(p, "u");
        } else if (abs_v < 0.9995f) { // milli
            dtostrf(abs_v * 1e3f, 1, 2, n_buf);
            p = append_str(p, n_buf); p = append_str(p, "m");
        } else { // Unità base
            dtostrf(abs_v, 1, 2, n_buf);
            p = append_str(p, n_buf);
        }
    } else { // Frequenza
        // ... (La logica K e M è corretta perché usa >=)
        if (abs_v >= 999500.0f) { // Mega
            dtostrf(abs_v / 1e6f, 1, 1, n_buf);
            p = append_str(p, n_buf); p = append_str(p, "M");
        } else if (abs_v >= 999.5f) { // Kilo
            dtostrf(abs_v / 1e3f, 1, 1, n_buf);
            p = append_str(p, n_buf); p = append_str(p, "K");
        } else {
            dtostrf(abs_v, 1, 1, n_buf);
            p = append_str(p, n_buf);
        }
    }
    append_str(p, unit);
}

/***************************************************************************************
** Function name:           calcola_e_stampa_dati_cursori
** Description:             Esegue i calcoli matematici per i cursori (Volt/Tempo) e
** aggiorna le etichette dell'interfaccia grafica. Include la
** gestione dei cambiamenti di scala e il rilevamento dinamico.
***************************************************************************************/
void calcola_e_stampa_dati_cursori() {
    if (cursor_type == CUR_OFF) return;

    static int16_t last_v_a, last_v_b, last_h_a, last_h_b;
    static float last_tdiv;
    static uint8_t last_type = 255, last_source = 255;

    Channel *ch = (cursor_source == 1) ? &ch1 : &ch2;
    float current_tdiv = timebase_seconds[current_time_base_idx];
    
    bool changed = (cursor_type != last_type) || (cursor_source != last_source) ||
                   (current_tdiv != last_tdiv);

    char bufA[24], bufB[24], bufD[24], bufF[24];

    if (cursor_type == CUR_VOLT) {
        if (cursor_v_a != last_v_a || cursor_v_b != last_v_b || changed) {
            float v_per_px = (ch->volts_div / 30.0f) * (ch->inverted ? -1.0f : 1.0f);
            float va = (float)(ch->offset - cursor_v_a) * v_per_px;
            float vb = (float)(ch->offset - cursor_v_b) * v_per_px;

            format_val_unit(bufA, va, 0, "V");
            format_val_unit(bufB, vb, 0, "V");
            format_val_unit(bufD, va - vb, 0, "V");

            drawTextButton(3, bufA, "", WHITE);
            drawTextButton(4, bufB, "", WHITE);
            drawTextButton(2, bufD, "", WHITE);
            last_v_a = cursor_v_a; last_v_b = cursor_v_b;
        }
    } 
    else if (cursor_type == CUR_TIME) {
        if (cursor_h_a != last_h_a || cursor_h_b != last_h_b || changed) {
            float t_per_px = current_tdiv / 40.0f;
            float ta = (float)(cursor_h_a - CENTER_TRACE_X) * t_per_px;
            float tb = (float)(cursor_h_b - CENTER_TRACE_X) * t_per_px;
            float dt = ta - tb;
            float abs_dt = (dt < 0) ? -dt : dt;

            format_val_unit(bufA, ta, 0, "s");
            format_val_unit(bufB, tb, 0, "s");
            format_val_unit(bufD, abs_dt, 0, "s");
            
            if (abs_dt > 1e-12f) format_val_unit(bufF, 1.0f / abs_dt, 1, "Hz");
            else append_str(bufF, "--- Hz");

            drawTextButton(3, bufA, "", WHITE);
            drawTextButton(4, bufB, "", WHITE);
            drawTextButton(2, bufD, bufF, WHITE);
            last_h_a = cursor_h_a; last_h_b = cursor_h_b;
        }
    }

    last_type = cursor_type; last_source = cursor_source; last_tdiv = current_tdiv;
}

/***************************************************************************************
** Function name:           aggiorna_grafica_cursori
** Description:             Gestisce il rendering visivo delle linee dei cursori sul display.
** Si occupa di cancellare le vecchie posizioni e disegnare le
** nuove (Orizzontali/Verticali) senza sporcare la traccia.
***************************************************************************************/
void aggiorna_grafica_cursori() {
    static uint8_t old_type = CUR_OFF;
    static int16_t ov_a = -1, ov_b = -1, oh_a = -1, oh_b = -1;

    // --- 1. GESTIONE CAMBIO MODALITÀ (Cancellazione totale precedente) ---
    if (cursor_type != old_type) {
        if (old_type == CUR_VOLT) {
            if (ov_a != -1) disegna_linea_cursore_v(ov_a, BLACK);
            if (ov_b != -1) disegna_linea_cursore_v(ov_b, BLACK);
        } 
        else if (old_type == CUR_TIME) {
            if (oh_a != -1) disegna_linea_cursore_h(oh_a, BLACK);
            if (oh_b != -1) disegna_linea_cursore_h(oh_b, BLACK);
        }
        old_type = cursor_type;
        ov_a = -1; ov_b = -1; oh_a = -1; oh_b = -1;
        if (cursor_type == CUR_OFF) return;
    }

    if (cursor_type == CUR_OFF) return;

    // --- 2. LOGICA DI DISEGNO/AGGIORNAMENTO ---
    if (cursor_type == CUR_VOLT) {
        // Cursore A
        if (cursor_v_a != ov_a) {
            if (ov_a != -1) disegna_linea_cursore_v(ov_a, BLACK); // Cancella solo se spostato
            ov_a = cursor_v_a;
        }
        disegna_linea_cursore_v(ov_a, CURSOR_COLOR); // Disegna SEMPRE nella posizione attuale

        // Cursore B
        if (cursor_v_b != ov_b) {
            if (ov_b != -1) disegna_linea_cursore_v(ov_b, BLACK);
            ov_b = cursor_v_b;
        }
        disegna_linea_cursore_v(ov_b, CURSOR_COLOR);
    } 
    else if (cursor_type == CUR_TIME) {
        // Cursore A
        if (cursor_h_a != oh_a) {
            if (oh_a != -1) disegna_linea_cursore_h(oh_a, BLACK);
            oh_a = cursor_h_a;
        }
        disegna_linea_cursore_h(oh_a, CURSOR_COLOR);

        // Cursore B
        if (cursor_h_b != oh_b) {
            if (oh_b != -1) disegna_linea_cursore_h(oh_b, BLACK);
            oh_b = cursor_h_b;
        }
        disegna_linea_cursore_h(oh_b, CURSOR_COLOR);
    }

    // Aggiorna i testi (che hanno già la loro logica interna di risparmio CPU)
    calcola_e_stampa_dati_cursori();
}

/***************************************************************************************
** Function name:           disegna_linea_cursore_v
** Description:             Disegna una linea orizzontale (asse Y) per i cursori di
** tensione. Gestisce il clipping all'interno dei margini
** dell'area di traccia per evitare artefatti nel menu.
***************************************************************************************/
void disegna_linea_cursore_v(int16_t y, uint16_t colore) {
    // Disegna una linea orizzontale tratteggiata (asse X: 0-400)
    for (int16_t x = 0; x < 400; x += 8) {
        tft_drawFastHLine(MARGIN_X + x, y, 4, colore); // Disegna 4 pixel, ne salta 4
    }
}

/***************************************************************************************
** Function name:           disegna_linea_cursore_h
** Description:             Disegna una linea verticale (asse X) per i cursori di
** tempo. Traccia la posizione temporale lungo l'intera
** altezza dell'area di visualizzazione della traccia.
***************************************************************************************/
void disegna_linea_cursore_h(int16_t x, uint16_t colore) {
    // Disegna una linea verticale tratteggiata (asse Y: 0-240)
    for (int16_t y = 0; y < 240; y += 8) {
        tft_drawFastVLine(x, MARGIN_Y + y, 4, colore);
    }
}