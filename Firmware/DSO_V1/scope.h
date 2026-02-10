#include <avr/io.h>
#ifndef SCOPE_H
#define SCOPE_H

#define F_SYS_CLK 40000000UL
#define REG_INDEX   _SFR_IO8(0x3C) //(*(volatile uint8_t*)0x81)
#define REG_CHA     _SFR_IO8(0x13)
#define REG_CHB     _SFR_IO8(0x14)
#define TRIG_CTRL_REG     _SFR_IO8(0x15)
#define REG_TRIG    _SFR_IO8(0x35)
#define REG_FREQ0     _SFR_IO8(0x00)
#define REG_FREQ1     _SFR_IO8(0x01)
#define REG_FREQ2     _SFR_IO8(0x02)
#define REG_FREQ3     _SFR_IO8(0x03)

#define REG_BASETIME        _SFR_IO8(0x14)
#define REG_TRIGGER_LEVEL   _SFR_IO8(0x13)
#define REG_TRIGGER_MODE    _SFR_IO8(0x15)


#define TRIG_CTRL_BIT  7         // il bit che sblocca wr_ptr


#define PRE_TRIGGER       200
#define POST_TRIGGER      200
#define BUFFER_TOTAL      (PRE_TRIGGER + POST_TRIGGER)
#define READY_BIT         1
#define BUFFER_SIZE       4096
#define DISPLAY_SAMPLES   255
#define PAN_LIMIT         200   
#define PAN_STEP          4

#define TRACE_W 400
#define TRACE_H 240
#define MARGIN_X 5
#define MARGIN_Y 25
#define SIDEBAR_X (MARGIN_X + TRACE_W + 5)

#define MENU_NONE       0
#define MENU_CH1        1
#define MENU_CH2        2
#define MENU_TRIG       3
#define MENU_TBASE      4
#define MENU_PAN        5

#define COUPL_DC  0
#define COUPL_AC  1
#define COUPL_GND 2

// Encoder mode
#define MODE_NONE       0
#define MODE_Y_POS      1
#define MODE_TRIG_LEVEL 2
#define MODE_TBASE      3
#define MODE_PAN        4

#define MAX_TIMEBASE_IDX 18

extern uint16_t _width;
extern uint16_t _height;
extern uint8_t  _rotation;

// Buffer interni
extern uint8_t buffer_a[BUFFER_TOTAL];
extern uint8_t buffer_b[BUFFER_TOTAL];
extern uint8_t buffer_c[BUFFER_TOTAL];

typedef enum {
    UI_STATUS_STOP = 0,
    UI_STATUS_WAIT,
    UI_STATUS_TRIGD,
    UI_STATUS_RUN
} ui_status_t;

// Modalità trigger
typedef enum {
    TRIG_MODE_AUTO = 0,
    TRIG_MODE_NORMAL = 1,
    TRIG_MODE_SINGLE = 2
} trigger_mode_t;

// Canale trigger
typedef enum {
    TRIG_CHAN_A = 0,
    TRIG_CHAN_B = 1
} trig_channel_t;

typedef enum {
    OSC_NOT_READY = 0,
    OSC_READY     = 1,
    OSC_TIMEOUT   = 2
} osc_status_t;

typedef enum {
    TRIG_SLOPE_RISING  = 0,
    TRIG_SLOPE_FALLING = 1
} trig_slope_t;

typedef enum {
    T_DIV  = 0,
    PAN = 1
} tdiv_pan_t;



// Funzioni di inizializzazione
//void osc_init_base_time(uint32_t base_time);
void osc_init_trigger(uint16_t trig_level, trigger_mode_t mode,
                      trig_channel_t chan, uint8_t edge_rising);

// Funzione per acquisizione dati (gestisce pre/post trigger)
//void osc_acquire_samples(void);

// Funzione di visualizzazione
//void osc_draw_samples(void);

void scope_main(void);


#endif