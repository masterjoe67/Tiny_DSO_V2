#ifndef FPGA_MANAGER_H
#define FPGA_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "scope_shared.h"

// --- Comandi FPGA ---
float read_fpga_frequency();
void set_trigger_mode(trigger_mode_t mode, trig_slope_t slope, uint8_t source);
void set_trigger_level(uint16_t level12);
void set_dds_frequency(uint32_t sample_rate);
void aggiorna_t_div(uint8_t indice);
void aggiorna_parametri_hw(Channel *ch);
void aggiorna_registri_dds(uint32_t reg_val);
void osc_write_view_offset(int16_t offset);
void set_base_time(uint8_t index);
void osc_read_triggered();
void osc_arm_readout(void);
#endif