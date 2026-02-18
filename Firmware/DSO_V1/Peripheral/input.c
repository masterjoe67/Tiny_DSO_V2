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


