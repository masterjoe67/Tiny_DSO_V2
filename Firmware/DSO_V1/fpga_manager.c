#include <stdint.h>
#include <stdlib.h>
#include <util/delay.h>
#include <stdbool.h>
#include <math.h>

#include "fpga_manager.h"
#include "scope_shared.h"

int16_t view_offset = 0;

void osc_arm_readout(void)
{
    REG_INDEX = 0;
}

float read_fpga_frequency() {
    static float history[FREQ_AVG_SAMPLES] = {0};
    static uint8_t idx = 0;
    static float sum = 0;
    static uint8_t count = 0;
    static float last_stable_freq = 0;

    if (is_running && !pan_flag && !freeze) {
        // Lettura registri (Assicurati che l'FPGA faccia il LATCH di tutti e 4 insieme)
        uint32_t period = ((uint32_t)REG_FREQ3 << 24) | 
                          ((uint32_t)REG_FREQ2 << 16) | 
                          ((uint32_t)REG_FREQ1 << 8)  | 
                           (uint32_t)REG_FREQ0;

        // Se il periodo è 0 o il watchdog dell'FPGA è intervenuto (FFFFFFFF)
        if (period == 0 || period == 0xFFFFFFFF) {
            // Svuota velocemente la media se non c'è segnale
            sum = 0; count = 0; idx = 0;
            for(int i=0; i<FREQ_AVG_SAMPLES; i++) history[i] = 0;
            last_stable_freq = 0;
            return 0;
        }

        // Calcolo frequenza istantanea
        // 3.840.000.000 = 60MHz * 64 (gate count)
        float instant_freq = 3840000000.0f / (float)period;

        // Filtro di plausibilità (evita spike assurdi)
        // Se la frequenza salta del 1000% in un colpo solo, ignoralo o gestiscilo
        if (count > 0 && instant_freq > last_stable_freq * 10.0f && last_stable_freq > 1.0f) {
             return last_stable_freq; 
        }

        // Aggiornamento Media Mobile
        sum -= history[idx];
        history[idx] = instant_freq;
        sum += history[idx];
        
        idx = (idx + 1) % FREQ_AVG_SAMPLES;
        if (count < FREQ_AVG_SAMPLES) count++;

        last_stable_freq = sum / count;
    } 
    
    return last_stable_freq;
}

void set_dds_frequency(uint32_t sample_rate) {
    // Calcolo del valore di incremento per il DDS
    // Formula: (F_sample / 60MHz) * 2^32
    // Usiamo i double per evitare l'overflow durante il calcolo
    double step = ((double)sample_rate / 60000000.0) * 4294967296.0;
    uint32_t reg_val = (uint32_t)step;

    // Invio dei 4 byte in sequenza
    // La FPGA li accatasta in dds_temp fino al 4° invio
    XRAM_WRITE(REG_DDS_ADDR, (uint8_t)(reg_val & 0xFF));         // Byte 0 (LSB)
    XRAM_WRITE(REG_DDS_ADDR, (uint8_t)((reg_val >> 8) & 0xFF));  // Byte 1
    XRAM_WRITE(REG_DDS_ADDR, (uint8_t)((reg_val >> 16) & 0xFF)); // Byte 2
    XRAM_WRITE(REG_DDS_ADDR, (uint8_t)((reg_val >> 24) & 0xFF)); // Byte 3 (MSB -> Trigger update)
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

void aggiorna_t_div(uint8_t indice) {
   if (indice > 19) indice = 19;

    uint32_t dds_val = pgm_read_dword(&dds_table[indice]);

    // Scrittura diretta sui 4 indirizzi mappati
    XRAM_WRITE(0x4020, (uint8_t)(dds_val & 0xFF));         // LSB
    XRAM_WRITE(0x4021, (uint8_t)((dds_val >> 8) & 0xFF));
    XRAM_WRITE(0x4022, (uint8_t)((dds_val >> 16) & 0xFF));
    XRAM_WRITE(0x4023, (uint8_t)((dds_val >> 24) & 0xFF)); // MSB
}

// Da chiamare SOLO quando l'utente cambia V/div o Offset
void aggiorna_parametri_hw(Channel *ch) {
    // 1. Calcolo della Scala (moltiplicata per 2^16 per matchare lo shift FPGA)
    // Se volts_div è 1V/div, f_scale sarà circa 16000.
    float f_scale = (3.3f / 4095.0f / ch->volts_div) * 30.0f * 65536.0f;
    
    if (ch->inverted) f_scale = -f_scale;

    int16_t hw_scale = (int16_t)f_scale;
    int16_t hw_offset = (int16_t)ch->offset; // Supporta da -50 a +250

    if (ch == &ch1) {
        // --- CANALE 1 ---
        XRAM_WRITE(REG_CH1_SCALE_L,  hw_scale & 0xFF);
        XRAM_WRITE(REG_CH1_SCALE_H,  hw_scale >> 8);
        XRAM_WRITE(REG_CH1_OFFSET_L, hw_offset & 0xFF);
        XRAM_WRITE(REG_CH1_OFFSET_H, hw_offset >> 8);
    } 
    else {
        // --- CANALE 2 ---
        XRAM_WRITE(REG_CH2_SCALE_L,  hw_scale & 0xFF);
        XRAM_WRITE(REG_CH2_SCALE_H,  hw_scale >> 8);
        XRAM_WRITE(REG_CH2_OFFSET_L, hw_offset & 0xFF);
        XRAM_WRITE(REG_CH2_OFFSET_H, hw_offset >> 8);
    }
}

void aggiorna_registri_dds(uint32_t reg_val) {
    // Scrittura diretta byte per byte agli indirizzi mappati nell'FPGA
    XRAM_WRITE(0x4020, (uint8_t)(reg_val & 0xFF));         // LSB
    XRAM_WRITE(0x4021, (uint8_t)((reg_val >> 8) & 0xFF));
    XRAM_WRITE(0x4022, (uint8_t)((reg_val >> 16) & 0xFF));
    XRAM_WRITE(0x4023, (uint8_t)((reg_val >> 24) & 0xFF)); // MSB
}

void osc_write_view_offset(int16_t offset)
{
    REG_BASETIME = 0xFF;                         // escape
    REG_BASETIME = 0x01;                         // comando: view_offset
    REG_BASETIME = (uint8_t)(offset & 0xFF);     // LSB
    REG_BASETIME = (uint8_t)(offset >> 8);       // MSB
    REG_BASETIME = 0xFF;

}

void set_base_time(uint8_t index) {
    // Limite di sicurezza aggiornato (0-20)
    if (index > MAX_TIMEBASE_IDX) index = MAX_TIMEBASE_IDX;
    
    current_time_base_idx = index;

    // Recuperiamo il valore DDS a 32 bit dalla tabella in Flash
    uint32_t dds_val = pgm_read_dword(&dds_table[index]);

    // Inviamo il valore all'FPGA usando la nuova funzione mappata
    aggiorna_registri_dds(dds_val);
    aggiorna_t_div(index);
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

