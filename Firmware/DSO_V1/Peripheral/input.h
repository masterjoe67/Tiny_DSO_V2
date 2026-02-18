#ifndef INPUT_H
#define INPUT_H

//#define F_CPU 16000000UL

#include <stdint.h>
#include <stdbool.h>

// -------------------------------------------------------------
//  CONFIGURA PIN (modifica secondo collegamento hardware)
// -------------------------------------------------------------
/*#define DB_REG   (*(volatile uint8_t *)0x30)
#define CLR_REG  (*(volatile uint8_t *)0x28)
#define MASK_REG  (*(volatile uint8_t *)0x32)
#define EVT_REG   (*(volatile uint8_t *)0x31)*/

#define KEY_STATUS   _SFR_IO8(0x10)
#define KEY_CODE     _SFR_IO8(0x11)
#define KEY_CTRL     _SFR_IO8(0x12)

#define KEY_VALID    (1 << 0)
#define KEY_REPEAT   (1 << 1)

#define KEY_REPEAT_EN (1 << 0)
#define KEY_CLEAR     (1 << 1)


// Indirizzi MMIO definiti nel tuo sistema
#define ENC_REG_CONF  _SFR_IO8(0x04) // Scrittura (Config)
#define ENC_REG_VAL_L _SFR_IO8(0x06) // Lettura (Byte basso)
#define ENC_REG_VAL_H _SFR_IO8(0x05) // Lettura (Byte alto)

// Identificatori dei Parametri (Bit 3:2 del primo byte)
#define PARAM_C_VAL  0x00
#define PARAM_MIN    0x01
#define PARAM_MAX    0x02
#define PARAM_STEP   0x03


#define ENC_VAL_L   (*(volatile uint8_t *)0x3c)
#define ENC_VAL_H  (*(volatile uint8_t *)0x3D)

typedef enum {
    KEY_RUN   = 0,
    KEY_MENU  = 1,
    KEY_AUTO  = 2,

    KEY_STOP  = 3,
    KEY_BACK  = 4,
    KEY_OK    = 5,

    KEY_PLUS  = 6,
    KEY_MINUS = 7,
    KEY_CURS  = 8,

    KEY_LEFT  = 9,
    KEY_RIGHT = 10,
    KEY_MEAS  = 11,

    KEY_T1    = 12,
    KEY_T2    = 13,
    KEY_SAVE  = 14
} keycode_t;


extern int16_t encoder_values[7];

void keypad_init(void);
uint8_t keypad_poll(uint8_t *key, uint8_t *repeat);


uint16_t encoder_read(void);

uint32_t update_param_32(uint32_t param, uint32_t min, uint32_t max, uint32_t step);

uint16_t update_param_16(uint16_t param, uint16_t min, uint16_t max, uint16_t step);
int16_t update_param_16_signed(int16_t param, int16_t min, int16_t max, int16_t step);
uint8_t update_param_8(uint8_t param, uint8_t min, uint8_t max, uint8_t step);



void configure_encoder(uint8_t id, uint8_t param, int16_t value) ;
int16_t read_encoder(uint8_t id);

// encoder movement: returns -1,0,+1 for rotation since last poll
int8_t encoder_get_delta(void);

#endif
