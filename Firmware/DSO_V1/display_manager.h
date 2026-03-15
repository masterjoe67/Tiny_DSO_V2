#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "scope_shared.h"

char* append_str(char* dest, const char* src) ;




void drawTextButton(uint8_t index, const char* data1, const char* data2, uint16_t color);
void update_status_bar(bool force);
ui_status_t get_system_status_code(void);
void draw_channel_status(Channel *ch, uint16_t xPos, uint16_t yPos, bool force);

void drawStaticInterface();

void drawMenuButton(uint8_t index, const char* label, const char* data, bool active, uint16_t color);
void updateSidebarLabels();


#endif // DISPLAY_MANAGER_H