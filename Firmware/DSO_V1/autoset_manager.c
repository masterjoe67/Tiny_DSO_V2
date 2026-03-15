#include <util/delay.h>
#include "autoset_manager.h"
#include "scope_shared.h"
#include "fpga_manager.h"
#include "display_manager.h"
#include "encoder_manager.h"

/*********************AUTOSET START *****************/

void init_timer_polling() {
// 1. Modalità Normale (Normal Mode)
    // Su ATmega128, mettendo TCCR0 a 0 si imposta la modalità normale 
    // e si scollegano i pin OC0 (no PWM).
    TCCR0 = 0; 

    // 2. Reset del contatore (8 bit: 0-255)
    TCNT0 = 0;

    // 3. Pulizia flag di overflow nel registro TIFR generico
    // Si scrive 1 sul bit TOV0 per resettarlo.
    TIFR |= (1 << TOV0);

    // 4. Impostazione Prescaler e avvio
    // A 60 MHz, per non far scappare il timer troppo velocemente,
    // usiamo il prescaler 256. 
    // Bit CS02=1, CS01=1, CS00=0 (secondo datasheet ATmega128)
    TCCR0 = (1 << CS02) | (1 << CS01);
}

uint8_t cerca_indice_timebase(float periodo) {
    // Se vogliamo vedere 4 cicli su 10 divisioni:
    // 1 divisione deve essere = (4 * periodo) / 10 = periodo * 0.4
    float t_div_ideale = periodo * 0.4f; 

    for (uint8_t i = 0; i < 21; i++) {
        // Appena troviamo una scala che è >= al nostro ideale, la prendiamo.
        // Essendo l'ideale basato su 4 cicli, anche se scegliamo quella 
        // immediatamente superiore, vedremo comunque tra 2 e 4 cicli.
        if (timebase_seconds[i] >= t_div_ideale) {
            return i;
        }
    }
    return 20; 
}

uint8_t cerca_indice_vdiv(float v_div_target) {
    // Scorriamo dal valore più sensibile (10mV) a quello più sordo (5V)
    for (uint8_t i = 0; i < 20; i++) {
        // Se il valore della tabella è maggiore o uguale a quello ideale,
        // abbiamo trovato la scala che contiene il segnale senza tagliarlo.
        if (v_div_values[i] >= v_div_target) {
            return i;
        }
    }
    return (20 - 1); // Default a 5V/div se il segnale è enorme
}

uint16_t get_vpp_raw_points(Channel *ch) {
    uint16_t max_adc = 0;
    uint16_t min_adc = 4095;
    
    // Puntiamo al buffer corretto in base al canale passato
    uint16_t *buffer = (ch == &ch1) ? ch1_buffer : ch2_buffer;

    // Analizziamo un numero sufficiente di campioni per beccare i picchi
    // 400 punti sono perfetti per coprire l'intera BRAM
    for (uint16_t i = 0; i < 400; i++) {
        uint16_t val = buffer[i] & 0x0FFF; // Pulizia 12-bit sempre presente

        if (val > max_adc) max_adc = val;
        if (val < min_adc) min_adc = val;
    }

    // Se il segnale è piatto o c'è un errore di inizializzazione
    if (max_adc <= min_adc) return 0;

    return (max_adc - min_adc);
}

void routine_autoset_dual() {
    draw_autoset_message(); // Mostriamo un messaggio chiaro all'utente durante l'autoset
    cursor_type = CUR_OFF;
    conf_encoder(); // Assicuriamoci che gli encoder siano configurati correttamente prima di leggere i valori
    // 1. Fase di Pre-analisi: Capire chi è vivo
    // Impostiamo una scala cautelativa per il check (es. 1V/div)
    current_time_base_idx = T_DIV_50US; 
    set_base_time(T_DIV_50US);
    write_encoder(4, T_DIV_50US);

    // 1. Fase di Pre-analisi
    ch1.vdiv_idx = V_DIV_1V; 
    ch2.vdiv_idx = V_DIV_1V;
    write_encoder(1, V_DIV_1V); 
    write_encoder(3, V_DIV_1V); 
    
    trigger_source = 1;
    trigger_mode = TRIG_MODE_AUTO; // Fondamentale per non bloccare l'FSM
    set_trigger_mode(TRIG_MODE_AUTO, TRIG_SLOPE_RISING, trigger_source);
    
    trigger_level_12bit = 2048;
    set_trigger_level(trigger_level_12bit);
    write_encoder(5, TRIG_C_VAL); 

    // --- RESET ACQUISIZIONE ---
    // Diamo un colpo di reset alla FSM dell'FPGA per farla partire con i nuovi dati
    //REG_TRIG = 0x00; 
    _delay_ms(1000);
    REG_TRIG = 0x01; 

    _delay_ms(50);

    bool signal_A = check_presence(&ch1);
    bool signal_B = check_presence(&ch2);

    // Se tutto è morto, torniamo a una visualizzazione standard
    if (!signal_A && !signal_B) {
        ch1.vdiv_idx = V_DIV_1V;
        ch2.vdiv_idx = V_DIV_1V;
        write_encoder(1, V_DIV_1V); // Aggiorna gli encoder per riflettere questa scelta
        write_encoder(3, V_DIV_1V); // Aggiorna gli encoder per riflettere questa scelta
        ch1.enabled = true;
        ch2.enabled = false;
        ch1.offset = OFFSET_Y1_C_VAL; // Centro schermo (assumendo 200px di altezza)
        write_encoder(1, V_DIV_1V); // Aggiorna gli encoder per riflettere questa scelta
        current_time_base_idx = T_DIV_10US;
        write_encoder(4, T_DIV_10US); // Aggiorna l'encoder della base dei tempi
        set_base_time(T_DIV_10US);
        tft_fillRect(MARGIN_X, MARGIN_Y, 400, 240, BLACK); // Pulisce tutto lo schermo per rimuovere i fantasmi dell'autoset 
        //REG_TRIG = 0x01;    // Armiamo la FSM FPGA

        return;
    }

    // 2. Configurazione Verticale (Scale e Posizioni)
    if (signal_A && signal_B) {
        // Attiviamo entrambi i canali
        ch1.enabled = true;
        ch2.enabled = true;

        // Calcoliamo scale e offset (85 per CH1, 205 per CH2)
        configura_verticale_smart(&ch1, 85); 
        configura_verticale_smart(&ch2, 205); 

        // IMPORTANTE: Devi aggiornare anche gli encoder degli offset (assumendo siano id 0 e 2)
        write_encoder(0, ch1.offset); 
        write_encoder(2, ch2.offset);
    } 
    else if (signal_A) {
        ch1.enabled = true;
        ch2.enabled = false;
        configura_verticale_smart(&ch1, 100); // Centro
        write_encoder(0, ch1.offset);
    }
    else if (signal_B) {
        ch1.enabled = false;
        ch2.enabled = true;
        configura_verticale_smart(&ch2, 100); // Centro
        write_encoder(2, ch2.offset);
    }

    // 3. Configurazione Orizzontale (Base Tempi)

    // Scegliamo la frequenza di riferimento per la base tempi
    float freq_ref = 0;
    /*if (signal_A) freq_ref = read_fpga_frequency_ch1(); // O la tua funzione freq
    else freq_ref = read_fpga_frequency_ch2();*/
    if(signal_A){ 
        set_trigger_mode(trigger_mode, trigger_slope, 1); // Agganciamo il trigger al canale 
        _delay_ms(100); // Tempo tecnico per stabilizzare la lettura dopo aver cambiato il trigger source
        //freq_ref = read_fpga_frequency(); // O la tua funzione freq
    }
    else {
        set_trigger_mode(trigger_mode, trigger_slope, 2); // Agganciamo il trigger al canale 
        _delay_ms(100); // Tempo tecnico per stabilizzare la lettura dopo aver cambi
        //freq_ref = read_fpga_frequency(); // O la tua funzione freq
    }

        // FORZIAMO UNA CATTURA FRESCA
    REG_TRIG = 0x01; // Arma l'FPGA

    // Aspettiamo che il READY_BIT salga (il frame deve essere nuovo)
    uint16_t timeout = 0;
    while (!(REG_TRIG & (1 << READY_BIT))) {
        _delay_us(50);
        timeout++;
        if (timeout > 2000) break; // 100ms timeout
    }

    // 3. LEGGIAMO i dati usando la tua funzione originale
    // Questo aggiorna ch1_buffer o ch2_buffer con dati REALI a 500mV/div
    attesa_campionamento_ms(1000); // Attende 500ms campionando continuamente (e aggiornando i buffer) per avere dati freschi e stabili

    freq_ref = read_fpga_frequency();


    if (freq_ref > 0.5f) {
        float periodo = 1.0f / freq_ref;
        // Vogliamo vedere circa 3 cicli su 10 divisioni
        float t_div_target = (3.0f * periodo) / 10.0f;
        
        // Cerchiamo l'indice che più si avvicina (funzione da definire sotto)
        current_time_base_idx = cerca_indice_timebase(t_div_target);
    } else {
        current_time_base_idx = T_BASE_10MS; // Default se segnale lentissimo
    }

    // 4. Configurazione Trigger
    // Agganciamo il trigger al canale principale trovato
    trigger_level_12bit = 2048; // Centrato (0V)
    if (signal_A) {
        trigger_source = 1;
    } else {
        trigger_source = 2;
    }

    set_trigger_level(trigger_level_12bit);
    write_encoder(5, TRIG_C_VAL); // Aggiorna l'encoder del trigger level
    trigger_mode = 0; // Imposta in AUTO per sicurezza
    trigger_slope = 0; // Rising edge
    set_trigger_mode(TRIG_MODE_AUTO, TRIG_SLOPE_RISING, trigger_source);

    // Alla fine di routine_autoset_dual()
    set_base_time(current_time_base_idx); // Invia la nuova base tempi all'FPGA
    write_encoder(4, current_time_base_idx);
    REG_TRIG = 0x01; // Riautomatizza l'acquisizione
    tft_fillRect(MARGIN_X, MARGIN_Y, 400, 240, BLACK); // Pulisce tutto lo schermo per rimuovere i fantasmi dell'autoset    
    if (signal_A) {
        currentMenu = MENU_CH1; // Se CH1 è vivo, diamo priorità a quello
    }
    else if (signal_B){
        currentMenu = MENU_CH2; // Altrimenti, diamo priorità a CH2
        
    }
    else {
        currentMenu = MENU_CH1; // Se non c'è nulla, torniamo al menu principale
    }
}

// Funzione di supporto per scalare il canale fino a riempire lo schermo
void configura_verticale_smart(Channel *ch, int target_offset) {
    ch->enabled = true;
    ch->offset = target_offset;
    
    // 1. CATTURA UNICA (L'hardware è fisso, basta una "foto")
    REG_TRIG = 0x01; 
    uint16_t timeout = 0;
    while (!(REG_TRIG & (1 << READY_BIT))) {
        _delay_us(10);
        if (++timeout > 5000) break;
    }
    osc_read_triggered();

    // 2. CALCOLO VPP REALE IN VOLT
    uint16_t v_max = 0;
    uint16_t v_min = 4095;
    uint16_t *buffer = (ch == &ch1) ? ch1_buffer : ch2_buffer;

    for(int n=0; n<400; n++) {
        uint16_t val = buffer[n] & 0x0FFF;
        if(val > v_max) v_max = val;
        if(val < v_min) v_min = val;
    }

    // Convertiamo i punti ADC in Volt (Delta totale)
    // 4096 punti : 10V = vpp_punti : vpp_volt
    float vpp_volt = ((float)(v_max - v_min) * 10.0f) / 4096.0f;

    // 3. SCELTA DELLA SCALA (Software Zoom)
    // Vogliamo che il segnale occupi circa 4 divisioni (4 * 30px = 120px)
    // Formula: pixel = (vpp_volt / volts_div) * 30
    // Quindi: volts_div = (vpp_volt * 30) / pixel_desiderati
    float v_div_ideale = (vpp_volt * 30.0f) / 120.0f;

    // Ora cerchiamo nella tua tabella dei VDIV l'indice che più si avvicina
    // (o il primo valore più grande di v_div_ideale per non saturare visivamente)
    ch->vdiv_idx = cerca_indice_vdiv(v_div_ideale);
    
    // Aggiorniamo il valore reale di volts_div nel canale
    ch->volts_div = v_div_values[ch->vdiv_idx];
    
    // Sincronizziamo l'interfaccia
    write_encoder((ch == &ch1) ? 1 : 3, ch->vdiv_idx);
}

void draw_autoset_message() {
    // Coordinate del box (ipotizzando uno schermo 320x240)
    // Regola questi valori in base alla tua risoluzione reale
    int16_t box_w = 140;
    int16_t box_h = 40;
    int16_t box_x = (400 - box_w) / 2; 
    int16_t box_y = (240 - box_h) / 2;

    // 1. Disegna il background del box (nero per coprire le tracce vecchie)
    tft_fillRect(box_x + MARGIN_X, box_y + MARGIN_Y, box_w, box_h, BLACK);

    // 2. Disegna la cornice (magari gialla o verde per dare l'idea di sistema attivo)
    tft_drawRect(box_x + MARGIN_X, box_y + MARGIN_Y, box_w, box_h, YELLOW);
    tft_drawRect(box_x + 2 + MARGIN_X, box_y + 2 + MARGIN_Y, box_w - 4, box_h - 4, YELLOW); // Doppia cornice pro

    // 3. Scritta "AUTOSET" centrata
    setTextSize(2); // Dimensione leggibile
    //tft_printAt("AUTOSET", box_x + 25 + MARGIN_X, box_y + 12 + MARGIN_Y, WHITE, DARKGREY, 2);
    tft_printCenteredX("AUTOSET", box_x + 2 + MARGIN_X, box_x + 2 + MARGIN_X + box_w - 4, box_y + 12 + MARGIN_Y, ORANGE, BLACK, 2);
    
    // Se la tua libreria lo richiede, forza l'invio al display
    // tft.display(); 
}

bool check_presence(Channel *ch) {
    // 1. Impostiamo la scala di "ascolto"
    ch->vdiv_idx = V_DIV_500MV; 
    write_encoder((ch == &ch1) ? 1 : 3, V_DIV_500MV); 
    
    // 2. FORZIAMO UNA CATTURA FRESCA
    REG_TRIG = 0x01; // Arma l'FPGA

    // Aspettiamo che il READY_BIT salga (il frame deve essere nuovo)
    uint16_t timeout = 0;
    while (!(REG_TRIG & (1 << READY_BIT))) {
        _delay_us(50);
        timeout++;
        if (timeout > 2000) break; // 100ms timeout
    }

    // 3. LEGGIAMO i dati usando la tua funzione originale
    // Questo aggiorna ch1_buffer o ch2_buffer con dati REALI a 500mV/div
    osc_read_triggered();

    // 4. Analisi del buffer aggiornato
    uint16_t vpp_raw = get_vpp_raw_points(ch);

    // Trasformiamo in Volt per decidere se c'è "vita"
    // (Uso get_vpp_raw_points perché l'hai già scritta ed è veloce)
    float lsb = (10.0f / 4096.0f) * ch->multiplier;
    float vpp = (float)vpp_raw * lsb;

    // Se vpp > 50mV, il canale è considerato attivo
    return (vpp > 0.050f);
}

void attesa_campionamento_ms(uint16_t ms) {
    // 1. Assicuriamoci che il Timer0 usi il clock di sistema (60MHz) e non l'RTC
    ASSR &= ~(1 << AS0); 

    for (uint16_t i = 0; i < ms; i++) {
        // 2. Prepariamo il timer
        TCNT0 = (256 - 234); // Pre-carico per 1ms (60MHz/256 prescaler)
        
        // Reset flag overflow: nell'ATmega128 si scrive 1 su TOV0 in TIFR
        TIFR |= (1 << TOV0); 

        // 3. Avvio Timer con Prescaler 256
        // Bit CS02=1, CS01=1, CS00=0 per ATmega128
        TCCR0 = (1 << CS02) | (1 << CS01); 

        // 4. Polling attivo
        while (!(TIFR & (1 << TOV0))) {
            osc_read_triggered();
            read_fpga_frequency();
            // Debug estremo: se non esce, stiamo leggendo il registro sbagliato
        }

        // 5. Fermiamo il timer
        TCCR0 = 0; 
    }
}