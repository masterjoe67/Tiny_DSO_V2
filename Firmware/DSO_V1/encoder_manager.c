#include "encoder_manager.h"
#include "scope_shared.h"
#include "fpga_manager.h"
#include "Peripheral/input.h"

EncoderMode current_enc_mode = ENC_MODE_TRIGGER_LEVEL;

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

void write_encoder(uint8_t encoder_idx, int16_t value) {
    switch (encoder_idx) {
        case 0: // Encoder 0 controlla la posizione verticale di CH1
            setup_encoder(0, value, OFFSET_Y_MIN, OFFSET_Y_MAX, OFFSET_Y_STEP);
            break;
        case 1: // Encoder 1 Volt/Div CH1
            setup_encoder(1, value, VDIVCH_MIN, VDIVCH_MAX, VDIVCH_STEP);
            break;    
        case 2: // Encoder 2 controlla la posizione verticale di CH2
            setup_encoder(2, value, OFFSET_Y_MIN, OFFSET_Y_MAX, OFFSET_Y_STEP); 
            break;
        case 3: // Encoder 1 Volt/Div CH1
            setup_encoder(3, value, VDIVCH_MIN, VDIVCH_MAX, VDIVCH_STEP);
            break;        
        case 4: // Encoder 4 controlla la base dei tempi
            setup_encoder(4, value, TDIV_MIN, TDIV_MAX, TDIV_STEP);
            break;
        case 5: // Encoder 5 controlla il livello di trigger
            setup_encoder(5, value, TRIG_MIN, TRIG_MAX, TRIG_STEP);
            break;
        case 6: // Encoder 6 controlla il Pan
            setup_encoder(6, value, -PAN_LIMIT, PAN_LIMIT, PAN_STEP);
            break;
        default:
            
            break;
    }
}

void set_trig_enc_default()
{
    current_enc_mode = ENC_MODE_TRIGGER_LEVEL;
    setup_encoder(5, trigger_level_12bit, TRIG_MIN, TRIG_MAX, TRIG_STEP);

}