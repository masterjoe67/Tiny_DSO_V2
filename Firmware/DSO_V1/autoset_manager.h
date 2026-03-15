#ifndef AUTOSET_MANAGER_H
#define AUTOSET_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "scope_shared.h"
/**
 * Funzione principale di Autoset.
 * Analizza il segnale in ingresso e imposta automaticamente 
 * Vdiv, Offset e Timebase.
 * * @return true se l'autoset è riuscito, false se segnale assente.
 */
void init_timer_polling();
uint8_t cerca_indice_timebase(float periodo);
uint8_t cerca_indice_vdiv(float v_div_target);
uint16_t get_vpp_raw_points(Channel *ch);
void routine_autoset_dual();
void configura_verticale_smart(Channel *ch, int target_offset);
void draw_autoset_message();
bool check_presence(Channel *ch);
void attesa_campionamento_ms(uint16_t ms);

#endif