#ifndef ENCODER_MANAGER_H
#define ENCODER_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

void conf_encoder();
void write_encoder(uint8_t encoder_idx, int16_t value);
void write_encoder(uint8_t encoder_idx, int16_t value);
void set_trig_enc_default();

#endif