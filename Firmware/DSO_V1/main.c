

#include <avr/io.h>
#include <util/delay.h>
#include <stdio.h>
#include "Peripheral/st7798.h"
#include "Peripheral/input.h"
#include "Peripheral/uart.h"
#include "Peripheral/leds.h"
//#include "scope.h"

// ------------------------------------------------
// Main
// ------------------------------------------------
int main(void) {
    uart_init(19200);
    uart_print("\r\nBoot AVR + ST7798S\r\n");

    uart_print("Inizializzo display...\r\n");

    //debounce_init(0xFF);   // abilita tutti i 7 pulsanti
    keypad_init();
    leds_init();
 
    tft_init();
    tft_setRotation(LANDSCAPE);
    tft_fillScreen(BLACK);

    setTextFont(2);
    setTextSize(1);
    setTextColor(WHITE, 0x0000);

    tft_drawFastVLine(50, 50, 50, GREEN);
    tft_drawFastHLine(100, 50, 100, RED);
    tft_drawLine(276, 95, 286, 75, CYAN);

    tft_drawPixel(120, 200, WHITE);

    //while(1);

    scope_main();

    return 0;
}
