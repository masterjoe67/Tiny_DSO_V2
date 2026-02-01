  //#define F_CPU 32000000UL
#include <avr/io.h>
#include <util/delay.h>
#include <stdio.h>
#include "uart.h"
  // UART ---------------------------------------
//#define BAUD 9600
#define MY_UBRR (F_CPU/16/BAUD-1)

void uart_init(uint32_t baud) {
    uint16_t ubrr_value = (F_CPU / (16UL * baud)) - 1;

    UBRR0H = (ubrr_value >> 8);
    UBRR0L = ubrr_value & 0xFF;

    // Abilita TX e RX
    UCSR0B = (1<<TXEN0) | (1<<RXEN0);
    
    // Formato frame: 8N1
    UCSR0C = (1<<UCSZ01) | (1<<UCSZ00);
}

void uart_tx(char c) {
    while(!(UCSR0A & (1<<UDRE0)));
    UDR0 = c;
}

void uart_print(const char *s) {
    while(*s) uart_tx(*s++);
}

void uart_putc(char c)
{
    // aspetta che il buffer di trasmissione sia vuoto
    while (!(UCSR0A & (1 << UDRE0)));

    // scrivi il byte nel registro di trasmissione
    UDR0 = c;
}

void uart_print_hex16(uint16_t v)
{
    const char hex[] = "0123456789ABCDEF";

    uart_putc(hex[(v >> 12) & 0x0F]);  // nibble alto del byte alto
    uart_putc(hex[(v >> 8)  & 0x0F]);  // nibble basso del byte alto
    uart_putc(hex[(v >> 4)  & 0x0F]);  // nibble alto del byte basso
    uart_putc(hex[v & 0x0F]);          // nibble basso del byte basso
}

void uart_print_int16(int16_t v)
{
    char buf[7];      // -32768\0
    uint8_t i = 0;

    if (v < 0) {
        uart_putc('-');
        v = -v;
    }

    do {
        buf[i++] = '0' + (v % 10);
        v /= 10;
    } while (v);

    while (i--)
        uart_putc(buf[i]);
}



void uart_print_hex(uint8_t v)
{
    const char hex[] = "0123456789ABCDEF";
    uart_putc(hex[(v >> 4) & 0x0F]);
    uart_putc(hex[v & 0x0F]);
}