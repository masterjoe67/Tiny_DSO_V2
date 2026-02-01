

#include <avr/io.h>
#include <util/delay.h>
#include <stdio.h>
#include "ili9341.h"
#include "Peripheral/input.h"
#include "Peripheral/uart.h"
#include "Peripheral/leds.h"
#include "scope.h"

// ------------------------------------------------
// Main
// ------------------------------------------------
int main(void) {

    DDRB |= (1<<PB0)|(1<<PB1)|(1<<PB2); // CS / RESET / DC
    CS_HIGH();

    spi_init();
    uart_init(19200);
    uart_print("\r\nBoot AVR + ILI9341\r\n");

    uart_print("Inizializzo display...\r\n");
	ILI9341_Init();
	ILI9341_Set_Rotation(3);
    //debounce_init(0xFF);   // abilita tutti i 7 pulsanti
    keypad_init();
    leds_init();
 
    setTextFont(2);
    setTextColor(ILI9341_WHITE,ILI9341_BLACK);

    scope_main();

    return 0;
}
