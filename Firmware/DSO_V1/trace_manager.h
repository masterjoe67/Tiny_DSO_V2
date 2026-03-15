#ifndef TRACE_MANAGER_H
#define TRACE_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "scope_shared.h"

void draw_dual_trace_from_bram(Channel *ch_a, Channel *ch_b, int16_t *old_buf_a, int16_t *old_buf_b, uint16_t length, bool vectors);
int16_t calcolaYTraccia(Channel *ch, uint16_t valoreADC_12bit, bool isTrigger);
void draw_trigger_line(uint16_t level12, uint16_t color, bool erase);
void tft_drawGrid(uint16_t color);
void draw_ground_marker(Channel *ch);
void drawPanTrack();
#endif