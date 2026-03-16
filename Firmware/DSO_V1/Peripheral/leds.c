#include "leds.h"

void LED_Init(void) {
    // Imposta i pin come output senza resettare l'intero registro DDRA
    DDRA |= (1 << LED_CH1_PIN) | (1 << LED_CH2_PIN) | (1 << LED_RS_GREEN) | (1 << LED_RS_RED);
    
    // Tutto spento all'avvio
    PORTA &= ~((1 << LED_CH1_PIN) | (1 << LED_CH2_PIN) | (1 << LED_RS_GREEN) | (1 << LED_RS_RED));
}

void LED_SetCH(uint8_t ch, uint8_t state) {
    uint8_t pin = (ch == 1) ? LED_CH1_PIN : LED_CH2_PIN;
    if (state) {
        PORTA |= (1 << pin);
    } else {
        PORTA &= ~(1 << pin);
    }
}

void LED_SetRunStop(RS_Color color) {
    // Puliamo i due pin del LED bicolor prima di impostare il nuovo colore
    PORTA &= ~((1 << LED_RS_GREEN) | (1 << LED_RS_RED));

    switch (color) {
        case LED_RED:
            PORTA |= (1 << LED_RS_RED);
            break;
        case LED_GREEN:
            PORTA |= (1 << LED_RS_GREEN);
            break;
        case LED_YELLOW:
            // Entrambi accesi = Giallo/Arancio
            PORTA |= (1 << LED_RS_RED) | (1 << LED_RS_GREEN);
            break;
        default:
            break;
    }
}