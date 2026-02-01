#include "input.h"
#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>
#include <stdbool.h>

/*void debounce_init(uint8_t mask) {
    MASK_REG = mask & 0xFF;
    (void)EVT_REG; // clear pending
}

uint8_t debounce_get_state(void) {
    return DB_REG & 0xFF;
}

uint8_t debounce_get_events(void) {
    return EVT_REG & 0xFF; // read clears pending events
}

void debounce_clear(uint8_t mask) {
    CLR_REG = mask & 0xFF;
}*/

void keypad_init(void)
{
    KEY_CTRL = 0;   // abilita auto-repeat
}


uint8_t keypad_poll(uint8_t *key, uint8_t *rep) {
    // 1. Leggiamo prima lo STATUS
    uint8_t status = KEY_STATUS; 

    // 2. Se il bit 0 è alto, c'è un tasto valido
    if (status & 0x01) { 
        // 3. Leggiamo il codice del tasto
        // Nel VHDL il flag key_valid si è azzerato appena abbiamo letto STATUS al punto 1
        // ma il valore in key_code_latched resta stabile fino al prossimo tasto.
        *key = KEY_CODE & 0x0F;
        *rep = (status >> 1) & 1;

        return 1; // Evento catturato
    }

    return 0; // Nessun tasto
}

//************************************************************************************** */
//*                           ENCODER
//************************************************************************************** */
int8_t encoder_get_delta(){
    static uint8_t prev = 0;
    uint8_t cur = ENC_VAL_L;   // macro o funzione MMIO

    int8_t delta = (int8_t)(cur - prev);

    prev = cur;
    return delta;
}

uint16_t encoder_read(void) {
    uint16_t val = ((uint16_t)ENC_VAL_H << 8) | ENC_VAL_L;
    return val;
}

static int16_t prev_det = 0;
int16_t prev_det_signed = 0;  // versione signed del detent

uint32_t update_param_32(uint32_t param, uint32_t min, uint32_t max, uint32_t step)
{
    int16_t pos = encoder_read();   // 0..1023

    // riduci a detent dividendo per 4
    int16_t det = pos;

    int16_t diff = det - prev_det;
    prev_det = det;

    if(diff == 0) return param;

    int32_t new_param = (int32_t)param + (int32_t)diff * step;

    if(new_param < min) new_param = min;
    if(new_param > max) new_param = max;

    return (uint32_t)new_param;
}



uint16_t update_param_16(uint16_t param, uint16_t min, uint16_t max, uint16_t step)
{
    int16_t pos = encoder_read();   // 0..1023

    // riduci a detent dividendo per 4
    int16_t det = pos;

    int16_t diff = det - prev_det;
    prev_det = det;

    if(diff == 0) return param;

    int16_t new_param = (int16_t)param + (int16_t)diff * step;

    if(new_param < min) new_param = min;
    if(new_param > max) new_param = max;

    return (uint16_t)new_param;
}



int16_t update_param_16_signed(int16_t param, int16_t min, int16_t max, int16_t step)
{
    int16_t pos = encoder_read();   // 0..1023

    // riduci a detent dividendo per 4 (o come vuoi)
    int16_t det = pos;

    int16_t diff = det - prev_det_signed;
    prev_det_signed = det;

    if(diff == 0) return param;

    int32_t new_param = (int32_t)param + (int32_t)diff * step; // usa int32_t per overflow temporaneo

    if(new_param < min) new_param = min;
    if(new_param > max) new_param = max;

    return (int16_t)new_param;
}


uint8_t update_param_8(uint8_t param, uint8_t min, uint8_t max, uint8_t step)
{
    int16_t pos = encoder_read();   // 0..1023

    // riduci a detent dividendo per 4
    int16_t det = pos;

    int16_t diff = det - prev_det;
    prev_det = det;

    if (diff > 0)
        diff = 1;
    else if (diff < 0)
        diff = -1;
    else
        return param;

    int8_t new_param = (int8_t)param + (int8_t)diff * step;

    if(new_param < min) new_param = min;
    if(new_param > max) new_param = max;

    return (uint8_t)new_param;
}