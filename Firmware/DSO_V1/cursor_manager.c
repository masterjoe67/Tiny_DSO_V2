#include <stdint.h>
#include <stdlib.h>
#include "cursor_manager.h"
#include "scope_shared.h"
#include "display_manager.h" // Per drawTextButton e append_str

// Variabili di stato cursori
CursorType cursor_type = CUR_OFF;
uint8_t cursor_source = 1; // 1 = CH1, 2 = CH2
uint8_t cursor_select = 0; // 0 = A, 1 = B, 2 = Entrambi

// Posizioni in pixel (0-400 o 0-240 a seconda dell'asse)
int16_t cursor_v_a = 50, cursor_v_b = 150; // Per Tensione
int16_t cursor_h_a = 100, cursor_h_b = 300; // Per Tempo

char str_ta[20];
char str_tb[20];

#define CURSOR_COLOR PINK

void calcola_e_stampa_dati_cursori() {
    if (cursor_type == CUR_OFF) return;

    // --- VARIABILI STATICHE PER MEMORIA ---
    static int16_t last_v_a = -32000, last_v_b = -32000;
    static int16_t last_h_a = -32000, last_h_b = -32000;
    static float last_vdiv1 = -1, last_vdiv2 = -1;
    static float last_tdiv = -1;
    static uint8_t last_type = 255;
    static uint8_t last_source = 255;

    // --- RILEVAMENTO CAMBIAMENTI ---
    Channel *ch = (cursor_source == 1) ? &ch1 : &ch2;
    bool changed = (cursor_type != last_type) || (cursor_source != last_source) ||
                   (ch->volts_div != (cursor_source == 1 ? last_vdiv1 : last_vdiv2)) ||
                   (timebase_seconds[current_time_base_idx] != last_tdiv);

    // --- LOGICA CURSORI VOLT ---
    if (cursor_type == CUR_VOLT) {
        if (cursor_v_a != last_v_a || cursor_v_b != last_v_b || changed) {
            float inv = (ch->inverted) ? -1.0 : 1.0;
            float v_per_px = (ch->volts_div / 30.0) * inv;
            float va = (float)(ch->offset - cursor_v_a) * v_per_px;
            float vb = (float)(ch->offset - cursor_v_b) * v_per_px;
            float dv = va - vb;
            char buf[24], val_str[12];
            char* p;

            // V(A)
            dtostrf(va, 1, 2, val_str);
            p = buf;
            *p++ = (va >= 0 ? '+' : ' '); 
            p = append_str(p, val_str);
            p = append_str(p, " V");
            drawTextButton(3, buf, "", WHITE);

            // V(B)
            dtostrf(vb, 1, 2, val_str);
            p = buf;
            *p++ = (vb >= 0 ? '+' : ' ');
            p = append_str(p, val_str);
            p = append_str(p, " V");
            drawTextButton(4, buf, "", WHITE);

            // Delta V
            dtostrf(dv, 1, 2, val_str);
            p = buf;
            *p++ = (dv >= 0 ? '+' : ' ');
            p = append_str(p, val_str);
            p = append_str(p, " V");
            drawTextButton(2, buf, "", WHITE);

            last_v_a = cursor_v_a; last_v_b = cursor_v_b;
        }
    } 
    // --- LOGICA CURSORI TEMPO ---
    else if (cursor_type == CUR_TIME) {
        if (cursor_h_a != last_h_a || cursor_h_b != last_h_b || changed) {
            float current_tb = timebase_seconds[current_time_base_idx]; 
            float t_per_px = current_tb / 40.0; 

            // --- CORREZIONE OFFSET TRIGGER ---
            // Modificato da 200 a 205 per compensare l'errore di +250ns a 2us/div
            const int16_t TRIGGER_PIXEL_ZERO = CENTER_TRACE_X; 

            float ta = (float)(cursor_h_a - TRIGGER_PIXEL_ZERO) * t_per_px;
            float tb = (float)(cursor_h_b - TRIGGER_PIXEL_ZERO) * t_per_px;
            float dt = ta - tb;
            float abs_dt = (dt < 0) ? -dt : dt;

            // T(A) e T(B)
            float tempi[2] = {ta, tb};
            char* targets[2] = {str_ta, str_tb};
            for(int i = 0; i < 2; i++) {
                float abs_t = (tempi[i] < 0) ? -tempi[i] : tempi[i];
                char n_buf[12];
                char* p = targets[i];
                *p++ = (tempi[i] < 0 ? '-' : '+');

                if (abs_t < 0.000001f) { // ns
                    dtostrf(abs_t * 1e9, 1, 0, n_buf);
                    p = append_str(p, n_buf);
                    append_str(p, " ns");
                } else if (abs_t < 0.001f) { // us
                    dtostrf(abs_t * 1e6, 1, 2, n_buf);
                    p = append_str(p, n_buf);
                    append_str(p, " us");
                } else { // ms
                    dtostrf(abs_t * 1e3, 1, 2, n_buf);
                    p = append_str(p, n_buf);
                    append_str(p, " ms");
                }
            }
            drawTextButton(3, str_ta, "", WHITE);
            drawTextButton(4, str_tb, "", WHITE);

            // Delta T
            char dt_buf[24], n_buf[12];
            char* p_dt = dt_buf;
            if (abs_dt < 0.000001f) {
                dtostrf(abs_dt * 1e9, 1, 0, n_buf);
                p_dt = append_str(p_dt, n_buf);
                append_str(p_dt, " ns");
            } else if (abs_dt < 0.001f) {
                dtostrf(abs_dt * 1e6, 1, 2, n_buf);
                p_dt = append_str(p_dt, n_buf);
                append_str(p_dt, " us");
            } else {
                dtostrf(abs_dt * 1e3, 1, 2, n_buf);
                p_dt = append_str(p_dt, n_buf);
                append_str(p_dt, " ms");
            }

            // Frequenza
            char f_buf[20], f_num[12];
            if (abs_dt > 1e-12f) { // Evitiamo divisione per zero quasi assoluto
                float freq = 1.0f / abs_dt;
                char* p_f = f_buf;
                if (freq >= 1000000.0f) {
                    dtostrf(freq / 1e6, 1, 1, f_num);
                    p_f = append_str(p_f, f_num);
                    append_str(p_f, " MHz");
                } else if (freq >= 1000.0f) {
                    dtostrf(freq / 1e3, 1, 1, f_num);
                    p_f = append_str(p_f, f_num);
                    append_str(p_f, " KHz");
                } else {
                    dtostrf(freq, 1, 1, f_num);
                    p_f = append_str(p_f, f_num);
                    append_str(p_f, " Hz");
                }
            } else {
                append_str(f_buf, "--- Hz");
            }
            
            drawTextButton(2, dt_buf, f_buf, WHITE);

            last_h_a = cursor_h_a; last_h_b = cursor_h_b;
        }
    }

    // --- AGGIORNAMENTO STATI ---
    last_type = cursor_type;
    last_source = cursor_source;
    last_vdiv1 = ch1.volts_div;
    last_vdiv2 = ch2.volts_div;
    last_tdiv = timebase_seconds[current_time_base_idx];
}

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

void disegna_linea_cursore_v(int16_t y, uint16_t colore) {
    // Disegna una linea orizzontale tratteggiata (asse X: 0-400)
    for (int16_t x = 0; x < 400; x += 8) {
        tft_drawFastHLine(MARGIN_X + x, y, 4, colore); // Disegna 4 pixel, ne salta 4
    }
}

void disegna_linea_cursore_h(int16_t x, uint16_t colore) {
    // Disegna una linea verticale tratteggiata (asse Y: 0-240)
    for (int16_t y = 0; y < 240; y += 8) {
        tft_drawFastVLine(x, MARGIN_Y + y, 4, colore);
    }
}