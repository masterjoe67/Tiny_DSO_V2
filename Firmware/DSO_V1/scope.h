#include <avr/io.h>
#include "scope_shared.h"

#ifndef SCOPE_H
#define SCOPE_H





/********************************************************* */
/*     KEY DEFINITION                                      */
/********************************************************* */

#define KEY_CH1        0x0D
#define KEY_CH2        0x0E
#define KEY_TRIGGER    0x0B
#define KEY_ORIZZONTAL 0x0A
#define KEY_SINGLE     0x07
#define KEY_RUN        0x08
#define KEY_CNTX1      0x0C
#define KEY_CNTX2      0x09
#define KEY_CURSORS    0x05
#define KEY_CNTX3      0x06
#define KEY_CNTX4      0x03
#define KEY_CNTX5      0x00
#define KEY_AUTOSET    0x02
#define KEY_MEASURE    0x01


extern uint16_t _width;
extern uint16_t _height;
extern uint8_t  _rotation;

// Buffer interni
extern uint8_t buffer_a[BUFFER_TOTAL];
extern uint8_t buffer_b[BUFFER_TOTAL];
//extern uint8_t buffer_c[BUFFER_TOTAL];


void scope_main(void);


#endif