#include "input.h"
#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>
#include <stdbool.h>

int16_t encoder_values[7] ;

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
        *key = KEY_CODE & 0x1F;
        *rep = (status >> 1) & 1;

        return 1; // Evento catturato
    }

    return 0; // Nessun tasto
}

//************************************************************************************** */
//*                           ENCODER
//************************************************************************************** */
void configure_encoder(uint8_t id, uint8_t param, int16_t value) {
    // Byte 1: [ID_ENC (4 bit) | PARAM (2 bit) | 00]
    // id: 0-6, param: 0-3
    ENC_REG_CONF = (uint8_t)((id << 4) | (param << 2));

    // Byte 2: Parte Alta (HI)
    ENC_REG_CONF = (uint8_t)(value >> 8);

    // Byte 3: Parte Bassa (LO) -> Qui l'FPGA applica il valore
    ENC_REG_CONF = (uint8_t)(value & 0xFF);
}


int16_t read_encoder(uint8_t id) {
    // 1. Seleziona l'encoder
    ENC_REG_CONF = (id << 4); 

    // 2. Leggi i registri (Assicurati che gli indirizzi siano corretti)
    uint8_t val_hi = ENC_REG_VAL_H; // Registro 3 (bit 15..8)
    uint8_t val_lo = ENC_REG_VAL_L; // Registro 2 (bit 7..0)

    /*uart_print( "ENC_REG_VAL_H: ");
        uart_print_hex(val_hi);
        uart_print( "\r\n");
    uart_print( "ENC_REG_VAL_L: ");
        uart_print_hex(val_lo);
        uart_print( "\r\n");*/

    // 3. Componi il valore: HI va a sinistra (moltiplicato 256), LO va a destra
    return (int16_t)((val_hi<< 8) | val_lo); 
}

void update_all_encoders() {
    for (uint8_t i = 0; i < 7; i++) {
        // 1. Seleziona l'encoder i-esimo (ID nei bit 7..4)
        // Scrivendo qui, config_state dell'FPGA va a 1
        ENC_REG_CONF = (i << 4);

        // 2. Lettura Byte Basso
        // Questa lettura (iore='1') resetta config_state dell'FPGA a 0
        uint8_t lo = ENC_REG_VAL_L;

        // 3. Lettura Byte Alto
        uint8_t hi = ENC_REG_VAL_H;

        // 4. Composizione valore a 16 bit con segno
        encoder_values[i] = (int16_t)((hi << 8) | lo);
    }
}
/************************************************************** */

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



/*int16_t update_param_16_signed(int16_t param, int16_t min, int16_t max, int16_t step)
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
}*/

int16_t update_param_16_signed(
    int16_t param,
    int16_t min,
    int16_t max,
    int16_t step
)
{
    int16_t det  = encoder_read();
    int16_t diff = det - prev_det_signed;
    prev_det_signed = det;

    /* nessun movimento */
    if (diff == 0)
        return param;

    /* filtro solo per glitch grossi (wrap / rumore) */
    if (diff > 20 || diff < -20)
        return param;

    /* calcolo SEMPRE in 32 bit */
    int32_t tmp = (int32_t)param + (int32_t)diff * (int32_t)step;

    /* clamp corretto */
    if (tmp < min) tmp = min;
    if (tmp > max) tmp = max;

    return (int16_t)tmp;
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