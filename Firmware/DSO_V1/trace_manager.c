#include <stdbool.h>
#include <math.h>

#include "trace_manager.h"
#include "scope_shared.h"

static int16_t last_trig_y = -100; // Inizializzato fuori schermo
static Point_t old_trig_a, old_trig_b, old_trig_c;

// Creiamo dei puntatori che puntano a quella zona
uint16_t *ch1_buffer = (uint16_t *)(RAM_EXTRA_START);
uint16_t *ch2_buffer = (uint16_t *)(RAM_EXTRA_START + 800); // 800 byte dopo (400 samples * 2)

// Puntatori ai buffer "storici" (vecchi dati per cancellazione)
int16_t *old_buffer_a = (int16_t *)(RAM_EXTRA_START + 1600);
int16_t *old_buffer_b = (int16_t *)(RAM_EXTRA_START + 2400);

Point_t old_a = { 0, 0 };
Point_t old_b = { 0, 0 };
Point_t old_c = { 0, 0 };
Point_t gnd_mark_a[2] = {{ 0, 0 }, { 0, 0 }};
Point_t gnd_mark_b[2] = {{ 0, 0 }, { 0, 0 }};
Point_t gnd_mark_c[2] = {{ 0, 0 }, { 0, 0 }};

/***************************************************************************************
** Function name:           calcola_XY_coordinata
** Description:             Mappa i valori ADC per la modalità XY. 
** is_x_axis = true  -> Mappa su larghezza (Area XY)
** is_x_axis = false -> Mappa su altezza (come calcolaYTraccia)
***************************************************************************************/
int16_t calcola_XY_coordinata(Channel *ch, uint16_t valoreADC_12bit, bool is_x_axis) {
    static const float RANGE_TOTALE = 10.0f; 
    static const float ADC_RESOLUTION = 4096.0f;
    static const float BITS_PER_VOLT = ADC_RESOLUTION / RANGE_TOTALE;
    static const float ADC_ZERO = 2048.0f;
    static const float PIXEL_PER_DIV = 30.0f;

    // 1. Calcolo del delta (identico alla tua funzione)
    float deltaADC = (float)valoreADC_12bit - ADC_ZERO;
    float volt = deltaADC / BITS_PER_VOLT;
    float pixelSpostamento = (volt / ch->volts_div) * PIXEL_PER_DIV;

    if (is_x_axis) {
        // --- ASSE X (Orizzontale) ---
        // Prendiamo l'offset del canale (che Joe muove con l'encoder)
        // e lo trasliamo dall'area verticale all'area orizzontale XY.
        // Se ch->offset è 120 (centro verticale), deve diventare 325 (centro XY)
        
        int16_t offset_traslato_x = ch->offset + (205 - MARGIN_Y); 
        
        if (ch->inverted) {
            return (int16_t)(offset_traslato_x - pixelSpostamento);
        } else {
            return (int16_t)(offset_traslato_x + pixelSpostamento);
        }
    } else {
        // --- ASSE Y (Verticale) ---
        // Usiamo esattamente la tua logica originale
        if (ch->inverted) {
            return (int16_t)(ch->offset - pixelSpostamento);
        } else {
            return (int16_t)(ch->offset + pixelSpostamento);
        }
    }
}

/***************************************************************************************
** Function name:           draw_trace
** Description:             Disegna e cancella la traccia usando la logica V/div
***************************************************************************************/
void draw_dual_trace_from_bram(Channel *ch_a, Channel *ch_b, int16_t *old_buf_a, int16_t *old_buf_b, uint16_t length, bool vectors)
{
    const int16_t Y_MIN = MARGIN_Y;
    const int16_t Y_MAX = MARGIN_Y + TRACE_H - 2;

    // --- 1. RESET VARIABILI MISURE (Locali, non static!) ---
    uint64_t sum_sq = 0; 
    int64_t sum_raw_acc = 0;
    int32_t max_adc = -1;    // Inizializzazione per forzare il primo aggiornamento
    int32_t min_adc = 4096; 
    uint16_t samples_misure = 0;

    // --- 2. CALCOLO MISURE (Ciclo lineare su tutto il buffer) ---
    if(misure->active) {
        for(uint16_t m = 0; m < 400; m++) {
            // Prendiamo il dato e forziamo la pulizia a 12 bit
            uint16_t val = (misure->source == 1) ? (uint16_t)ch1_buffer[m] : (uint16_t)ch2_buffer[m];
            val &= 0x0FFF; 

            // Aggiornamento picchi: solo se il valore è nel range reale dell'ADC
            if ((int32_t)val > max_adc) max_adc = (int32_t)val;
            if ((int32_t)val < min_adc) min_adc = (int32_t)val;

            // Accumulo per Vavg (media)
            sum_raw_acc += val;

            // Accumulo per Vrms centrato su 2048
            int32_t centrato = (int32_t)val - 2048;
            sum_sq += (uint64_t)((int64_t)centrato * centrato);
            
            samples_misure++;
        }
    }

    // --- 3. LOGICA TIMING DISEGNO (Invariata) ---
    uint32_t step_fp; uint32_t ram_idx_fp;   
    if (current_time_base_idx == 0) { step_fp = (150UL << 8) / length; ram_idx_fp = 125UL << 8; } 
    else if (current_time_base_idx == 1) { step_fp = (300UL << 8) / length; ram_idx_fp = 50UL << 8; } 
    else { step_fp = 1UL << 8; ram_idx_fp = 0; }

    int16_t y_prev_new_a = -100, y_prev_old_a = -100;
    int16_t y_prev_new_b = -100, y_prev_old_b = -100;


   // --- 4. LOOP DI DISEGNO (Cancellazione SEMPRE, Tracciamento solo se ENABLED) ---
    for (uint16_t i = 0; i < length; i++) {
        uint16_t x = i + MARGIN_X;
        uint16_t ram_idx = (uint16_t)(ram_idx_fp >> 8);
        if (ram_idx >= 400) ram_idx = 399;

        // --- GESTIONE CANALE A ---
        // 1. CANCELLAZIONE (Sempre, se il vecchio punto era valido)
        if (old_buf_a[i] > Y_MIN && old_buf_a[i] < Y_MAX) {
            if (vectors && i > 0 && y_prev_old_a > Y_MIN) 
                tft_drawLine_Clipped(x-1, y_prev_old_a, x, old_buf_a[i], BLACK, Y_MIN, Y_MAX);
            else 
                tft_drawPixel(x, old_buf_a[i], BLACK);
        }
        y_prev_old_a = old_buf_a[i];

        // 2. DISEGNO NUOVO PUNTO (Solo se abilitato)
        if (ch_a->enabled) {
            int16_t y_now_a = calcolaYTraccia(ch_a, ch1_buffer[ram_idx], false);
            if (y_now_a > Y_MIN && y_now_a < Y_MAX) {
                if (vectors && i > 0 && y_prev_new_a > Y_MIN) 
                    tft_drawLine_Clipped(x-1, y_prev_new_a, x, y_now_a, ch_a->color, Y_MIN, Y_MAX);
                else 
                    tft_drawPixel(x, y_now_a, ch_a->color);
            }
            y_prev_new_a = y_now_a;
            old_buf_a[i] = y_now_a; // Salva per cancellarlo al prossimo giro
        } else {
            old_buf_a[i] = -100;    // Segna come vuoto per i prossimi cicli
            y_prev_new_a = -100;
        }

        // --- GESTIONE CANALE B ---
        // 1. CANCELLAZIONE
        if (old_buf_b[i] > Y_MIN && old_buf_b[i] < Y_MAX) {
            if (vectors && i > 0 && y_prev_old_b > Y_MIN) 
                tft_drawLine_Clipped(x-1, y_prev_old_b, x, old_buf_b[i], BLACK, Y_MIN, Y_MAX);
            else 
                tft_drawPixel(x, old_buf_b[i], BLACK);
        }
        y_prev_old_b = old_buf_b[i];

        // 2. DISEGNO NUOVO PUNTO
        if (ch_b->enabled) {
            int16_t y_now_b = calcolaYTraccia(ch_b, ch2_buffer[ram_idx], false);
            if (y_now_b > Y_MIN && y_now_b < Y_MAX) {
                if (vectors && i > 0 && y_prev_new_b > Y_MIN) 
                    tft_drawLine_Clipped(x-1, y_prev_new_b, x, y_now_b, ch_b->color, Y_MIN, Y_MAX);
                else 
                    tft_drawPixel(x, y_now_b, ch_b->color);
            }
            y_prev_new_b = y_now_b;
            old_buf_b[i] = y_now_b;
        } else {
            old_buf_b[i] = -100;
            y_prev_new_b = -100;
        }

        ram_idx_fp += step_fp;
    }

    // --- 5. CALCOLO FINALE MISURE (Senza ambiguità) ---
    if(misure->active && samples_misure > 0) {
        Channel *ch_src = (misure->source == 1) ? ch_a : ch_b;
        const float LSB = (10.0f / 4096.0f) * ch_src->multiplier;

        // Calcolo Vpp: usiamo int32_t per la differenza e poi forziamo il valore assoluto
        int32_t delta = max_adc - min_adc;
        if (delta < 0) delta = 0; // Protezione contro inizializzazioni fallite
        misure->vpp = (float)delta * LSB;

        // Vavg
        float avg_raw = ((float)sum_raw_acc / (float)samples_misure) - 2048.0f;
        misure->vavg = avg_raw * LSB;

        // Vrms
        float rms_pts = sqrtf((float)sum_sq / (float)samples_misure);
        misure->vrms = rms_pts * LSB;
    }
}


/***************************************************************************************
** Function name:           draw_xy_trace
** Description:             Rendering della traccia in modalità X-Y (Lissajous).
** Utilizza il CH1 come asse X e il CH2 come asse Y. La funzione 
** gestisce la cancellazione selettiva dei vecchi pixel tramite 
** buffer di memoria per garantire fluidità senza flickering.
** Parameters:              ch_x, ch_y: puntatori ai canali sorgente
** old_x_buf, old_y_buf: buffer storici per la cancellazione
** length: numero di campioni da processare
***************************************************************************************/
void draw_xy_trace(Channel *ch_x, Channel *ch_y, int16_t *old_x_buf, int16_t *old_y_buf, uint16_t length) 
{
    // Confini dell'area XY quadrata
    const int16_t OFFSET_X = OFFSET_XY_AREA; 
    const int16_t X_MIN = OFFSET_X;
    const int16_t X_MAX = OFFSET_X + 240;
    const int16_t Y_MIN = MARGIN_Y;
    const int16_t Y_MAX = MARGIN_Y + 240;

    for (uint16_t i = 0; i < length; i++) {
        // --- 1. CANCELLAZIONE (Usa i buffer esistenti) ---
        if (old_x_buf[i] != -100) {
            tft_drawPixel(old_x_buf[i], old_y_buf[i], BLACK);
        }

        // --- 2. CALCOLO NUOVO PUNTO ---
        if (ch_x->enabled && ch_y->enabled) {
            
            // Usiamo la TUA funzione calcolaYTraccia che funziona bene
            // Per la Y è diretta:
            int16_t raw_y = calcolaYTraccia(ch_y, ch2_buffer[i], false);
            
            // Per la X usiamo la stessa funzione, ma dobbiamo "spostarla"
            // perché calcolaYTraccia restituisce una coordinata verticale (Y).
            // Noi sottraiamo il suo margine verticale e aggiungiamo quello orizzontale XY.
            int16_t raw_x = calcolaYTraccia(ch_x, ch1_buffer[i], false);
            int16_t new_x = raw_x - MARGIN_Y + OFFSET_X;
            int16_t new_y = raw_y;

            // --- 3. DISEGNO E SALVATAGGIO ---
            // Verifichiamo che il punto sia dentro il quadrato 240x240
            if (new_x >= X_MIN && new_x < X_MAX && new_y >= Y_MIN && new_y < Y_MAX) {
                tft_drawPixel(new_x, new_y, GREEN);
                old_x_buf[i] = new_x;
                old_y_buf[i] = new_y;
            } else {
                old_x_buf[i] = -100; // Segnaposto fuori schermo
            }
        }
    }
}

/***************************************************************************************
** Function name:           calcolaYTraccia
** Description:             Converte il valore ADC in coordinata Y pixel basandosi
** sui parametri del canale (V/div, offset, invert).
***************************************************************************************/
int16_t calcolaYTraccia(Channel *ch, uint16_t valoreADC_12bit, bool isTrigger) {
    static const float RANGE_TOTALE = 10.0f; 
    static const float ADC_RESOLUTION = 4096.0f;
    static const float BITS_PER_VOLT = ADC_RESOLUTION / RANGE_TOTALE; // 409.6
    static const float ADC_ZERO = 2048.0f;
    static const float PIXEL_PER_DIV = 30.0f;

    // 1. Delta rispetto al centro
    float deltaADC = (float)valoreADC_12bit - ADC_ZERO;

    // 2. Volt reali
    float volt = deltaADC / BITS_PER_VOLT;

    // 3. Quanti pixel si deve spostare (es. 1V = 2 divisioni = 60 pixel)
    float pixelSpostamento = (volt / ch->volts_div) * PIXEL_PER_DIV;

    // --- CORREZIONE INVERSIONE ---
    // Se il segnale è positivo (volt > 0), vogliamo che la Y diminuisca 
    // per andare verso l'alto dello schermo.
    if (ch->inverted) {
        return (int16_t)(ch->offset - pixelSpostamento); 
    } else {
        return (int16_t)(ch->offset + pixelSpostamento);
    }
}

void draw_trigger_line(uint16_t level12, uint16_t color, bool erase) {
    Channel *trig_ch = (trigger_source == 1) ? &ch1 : &ch2;
    int16_t y = calcolaYTraccia(trig_ch, level12, true); 
    
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

/***************************************************************************************
** Function name:           tft_drawGrid_XY
** Description:             Disegna la griglia specifica per la modalità X-Y.
** Crea un quadrato 240x240 con croce centrale e divisioni puntinate.
***************************************************************************************/
void tft_drawGrid_XY(uint16_t color) {
    const int16_t OFFSET_X = OFFSET_XY_AREA;
    const int16_t SIZE = 240;
    
    int16_t xStart = OFFSET_X;
    int16_t yStart = MARGIN_Y;
    int16_t xEnd   = OFFSET_X + SIZE;
    int16_t yEnd   = MARGIN_Y + SIZE;

    // Spaziatura divisioni: 30 pixel (per avere 8 divisioni in 240px)
    uint8_t spacing = 30; 
    uint8_t dotSpacing = 6;

    int16_t xCenter = xStart + (SIZE / 2);
    int16_t yCenter = yStart + (SIZE / 2);

    // 1. Cornice esterna del quadrato XY
    tft_drawRect(xStart, yStart, SIZE, SIZE, color);

    // 2. Assi Centrali (Croce Continua)
    // Asse Orizzontale (X)
    for (int16_t x = xStart; x <= xEnd; x += 2) {
        tft_drawPixel(x, yCenter, color);
    }
    // Asse Verticale (Y)
    for (int16_t y = yStart; y <= yEnd; y += 2) {
        tft_drawPixel(xCenter, y, color);
    }

    // 3. Divisioni Interne (Puntinate)
    // Linee Orizzontali puntinate
    for (int16_t y = yStart + spacing; y < yEnd; y += spacing) {
        if (y == yCenter) continue; // Salta l'asse già disegnato
        for (int16_t x = xStart; x <= xEnd; x += dotSpacing) {
            tft_drawPixel(x, y, color);
        }
    }

    // Linee Verticali puntinate
    for (int16_t x = xStart + spacing; x < xEnd; x += spacing) {
        if (x == xCenter) continue; // Salta l'asse già disegnato
        for (int16_t y = yStart; y <= yEnd; y += dotSpacing) {
            tft_drawPixel(x, y, color);
        }
    }
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

