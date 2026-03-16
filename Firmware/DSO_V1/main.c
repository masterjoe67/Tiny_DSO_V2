

#include <avr/io.h>
#include <util/delay.h>
#include <stdio.h>
#include "Peripheral/st7798.h"
#include "Peripheral/input.h"
#include "Peripheral/uart.h"
#include "Peripheral/leds.h"
#include "scope.h"

// ------------------------------------------------
// Main
// ------------------------------------------------
int main(void) {
    uart_init(19200);
    uart_print("\r\nBoot AVR + ST7798S\r\n");

    uart_print("Inizializzo display...\r\n");

    keypad_init();
    LED_Init();
 
    tft_init();
    tft_set_backlight(128);
    tft_setRotation(3);
    tft_fillScreen(BLACK);

    setTextFont(2);
    setTextSize(1);
    setTextColor(WHITE, 0x0000);

    scope_main();

    return 0;
}
