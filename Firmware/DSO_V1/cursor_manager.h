#ifndef CURSOR_MANAGER_H
#define CURSOR_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

void calcola_e_stampa_dati_cursori();
void aggiorna_grafica_cursori();
void disegna_linea_cursore_v(int16_t y, uint16_t colore);
void disegna_linea_cursore_h(int16_t x, uint16_t colore);

#endif