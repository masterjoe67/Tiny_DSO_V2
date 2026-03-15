#ifndef TRIGGER_MANAGER_H
#define TRIGGER_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

uint16_t calcola_step_trigger(float current_vdiv);

void scope_set_hysteresis(uint8_t value);

#endif