#ifndef LED_CONTROL_H
#define LED_CONTROL_H

#include <avr/io.h>

// Definizioni Pin
#define LED_CH1_PIN    PA0
#define LED_CH2_PIN    PA1
#define LED_RS_GREEN   PA2
#define LED_RS_RED     PA3

// Stati per il LED bicolor
typedef enum {
    LED_OFF = 0,
    LED_RED,
    LED_GREEN,
    LED_YELLOW
} RS_Color;

// Funzioni
void LED_Init(void);
void LED_SetCH(uint8_t ch, uint8_t state); // 1 per CH1, 2 per CH2
void LED_SetRunStop(RS_Color color);

#endif