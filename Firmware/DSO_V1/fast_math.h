#ifndef FAST_MATH_H
#define FAST_MATH_H

#include <stdint.h>
//#include <avr/pgmspace.h>

// Sostituto leggero di fabs()
// Se x è negativo, lo inverte. Lavora su interi a 16 o 32 bit.
#define fm_abs(x) ((x) < 0 ? -(x) : (x))

uint16_t fm_sqrt(uint32_t n);

// Calcola il modulo (Magnitude) di un numero complesso: sqrt(re^2 + im^2)
// Usa l'algoritmo Alpha Max + Beta Min (errore max ~4%)
uint16_t fm_mag(int16_t re, int16_t im);

// Approssimazione del logaritmo in base 10 scalato (per calcolo dB)
// Restituisce un valore utile per il mapping su display
uint8_t fm_log10(uint32_t value);

// Restituisce il seno di un angolo (0-360 gradi) scalato a 127
// Usa una tabella PROGMEM per risparmiare ROM
int8_t fm_sin(uint16_t angle);

// Restituisce il cosen di un angolo (0-360 gradi)
#define fm_cos(a) fm_sin((a) + 90)

#endif