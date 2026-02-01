#include <avr/io.h>
#include <util/delay.h>
#include <stdio.h>

  void uart_init(uint32_t baud);
  void uart_tx(char c);
  void uart_print(const char *s);
  void uart_putc(char c);
  void uart_print_hex16(uint16_t v);
  void uart_print_hex(uint8_t v);
  void uart_print_int16(int16_t v);
  