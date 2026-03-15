#ifndef SCOPE_SHARED_H
#define SCOPE_SHARED_H

#include <stdint.h>
#include <stdbool.h>
#include "Peripheral/st7798.h"

#define F_SYS_CLK 60000000UL
#define REG_INDEX   _SFR_IO8(0x3C) //(*(volatile uint8_t*)0x81)
#define REG_CHA     _SFR_IO8(0x13)
#define REG_CHB     _SFR_IO8(0x14)
#define TRIG_CTRL_REG     _SFR_IO8(0x15)
#define REG_TRIG    _SFR_IO8(0x35)
#define REG_FREQ0     _SFR_IO8(0x00)
#define REG_FREQ1     _SFR_IO8(0x01)
#define REG_FREQ2     _SFR_IO8(0x02)
#define REG_FREQ3     _SFR_IO8(0x03)
#define INDEX_RESET   _SFR_IO8(0x1C)
#define REG_BASETIME        _SFR_IO8(0x14)
#define REG_TRIGGER_LEVEL   _SFR_IO8(0x13)
#define REG_TRIGGER_MODE    _SFR_IO8(0x15)

#define BRAM_START_ADDR 0x4000
#define BRAM_DATA_PTR   ((volatile uint8_t *)0x4000)

// Definiamo l'inizio della RAM "extra" (dopo i primi 4KB dell'ATmega128)
// L'indirizzo 0x1100 è l'inizio della zona oltre i 4KB standard
#define RAM_EXTRA_START 0x1100
#define ADDR_OLD_A (int16_t*)(0x1740)
#define ADDR_OLD_B (int16_t*)(0x1A60)

// Indirizzi base del Co-Processore
// --- CONFIGURAZIONE CANALE 1 ---
#define REG_CH1_SCALE_L   0x4010  // Scrittura: Scala bit 7-0
#define REG_CH1_SCALE_H   0x4011  // Scrittura: Scala bit 15-8
#define REG_CH1_OFFSET_L  0x4012  // Scrittura: Offset bit 7-0
#define REG_CH1_OFFSET_H  0x4013  // Scrittura: Offset bit 15-8

// --- CONFIGURAZIONE CANALE 2 ---
#define REG_CH2_SCALE_L   0x4014  // Scrittura: Scala bit 7-0
#define REG_CH2_SCALE_H   0x4015  // Scrittura: Scala bit 15-8
#define REG_CH2_OFFSET_L  0x4016  // Scrittura: Offset bit 7-0
#define REG_CH2_OFFSET_H  0x4017  // Scrittura: Offset bit 15-8

// --- INPUT ADC E TRIGGER CALCOLO ---
#define REG_CH1_ADC_L     0x4018  // Scrittura: Byte basso ADC (Parcheggio)
#define REG_CH1_ADC_H     0x4019  // Scrittura: Byte alto ADC (Trigger Calcolo CH1)
#define REG_CH2_ADC_L     0x401A  // Scrittura: Byte basso ADC (Parcheggio)
#define REG_CH2_ADC_H     0x401B  // Scrittura: Byte alto ADC (Trigger Calcolo CH2)

// --- RISULTATO CALCOLO (LETTURA) ---
#define REG_Y_RESULT_L    0x401C  // Lettura: y_result bit 7-0
#define REG_Y_RESULT_H    0x401D  // Lettura: y_result bit 15-8

#define REG_TRIG_HYST    0x401E  // Scrittura: Trigger Hysteresis (0-255, in LSB)

#define REG_DDS_ADDR 0x401F  // Frequenza di uscita del DDS (4 byte, 32 bit unsigned int)

#define XRAM_WRITE(addr, val) (*(volatile uint8_t *)(addr) = (val))
#define XRAM_READ(addr) (*(volatile uint8_t *)(addr))

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
#define CENTER_TRACE_X (MARGIN_X + (TRACE_W / 2))

#define COUPL_DC  0
#define COUPL_AC  1
#define COUPL_GND 2

#define FREQ_AVG_SAMPLES 32

#define MENU_NONE       0
#define MENU_CH1        1
#define MENU_CH2        2
#define MENU_TRIG       3
#define MENU_MEAS       4
#define MENU_PAN        5
#define MENU_CURSORS    6

// Valori di default per gli encoder (minimo, massimo step, valore iniziale)
#define OFFSET_Y_MIN -50
#define OFFSET_Y_MAX 250
#define OFFSET_Y_STEP 2
#define OFFSET_Y1_C_VAL 145
#define OFFSET_Y2_C_VAL 145
#define POSIZIONE_TOP 115
#define POSIZIONE_BOTTOM 175
#define POSIZIONE_CENTER 145

#define VDIVCH_MIN 0
#define VDIVCH_MAX 9
#define VDIVCH_STEP 1
#define VDIVCH_C_VAL 5

#define TDIV_MIN 0
#define TDIV_MAX 20
#define TDIV_STEP 1
#define TDIV_C_VAL 11

#define TRIG_MIN 0
#define TRIG_MAX 4095
#define TRIG_STEP 64
#define TRIG_C_VAL 2048

#define MAX_TIMEBASE_IDX 18
#define MAX_VDIV_IDX 9

#define PRE_TRIGGER       200
#define POST_TRIGGER      200

// Parametri reticolo
#define GRID_SPACING 30     // distanza tra linee
#define DOT_SPACING 4       // distanza tra puntini
#define COLOR_GRID WHITE


#define V_DIV_10MV 0
#define V_DIV_20MV 1
#define V_DIV_50MV 2
#define V_DIV_100MV 3
#define V_DIV_200MV 4
#define V_DIV_500MV 5
#define V_DIV_1V 6
#define V_DIV_5V 8
#define V_DIV_10V 9

#define T_DIV_250NS 0
#define T_DIV_500NS 1
#define T_DIV_1US 2
#define T_DIV_2US 3
#define T_DIV_5US 4
#define T_DIV_10US 5
#define T_DIV_20US 6
#define T_DIV_50US 7
#define T_DIV_100US 8
#define T_DIV_200US 9
#define T_DIV_500US 10
#define T_BASE_1MS 11
#define T_BASE_10MS 12

typedef struct {
    uint8_t enabled;      // 0 = Spento, 1 = Acceso
    uint8_t focused;      // 1 = Il menu attuale è dedicato a questo canale
    float volts_div;      // Scala verticale
    float old_volts_div;  // Per rilevare cambiamenti e aggiornare a schermo
    int16_t offset;       // Posizione verticale sullo schermo
    uint8_t inverted;     // 0 = Normale, 1 = Invertito
    uint8_t coupling; // Tipo di accoppiamento (DC, AC, GND)
    uint8_t old_coupling; // Per rilevare cambiamenti e aggiornare a schermo
    uint8_t vdiv_idx;     // Indice per il Volt/Div (0-9)
    uint8_t old_vdiv_idx; // Per rilevare cambiamenti e aggiornare a
    uint8_t probe;       // Tipo di sonda (1X, 10X, 100X)
    uint8_t bw_limit;  // 0: Full BW, 1: 20MHz Limit
    float multiplier;    // Fattore di moltiplicazione per calcolare la tensione reale
    float old_multiplier; // Per rilevare cambiamenti e aggiornare a schermo
    uint8_t isFine;
    uint16_t old_offset;
    uint16_t color;        // Colore della traccia
    Point_t gnd_mark_a;
    Point_t gnd_mark_b;
    Point_t gnd_mark_c;

} Channel;

typedef enum {
    UI_STATUS_STOP = 0,
    UI_STATUS_WAIT,
    UI_STATUS_TRIGD,
    UI_STATUS_RUN
} ui_status_t;

typedef struct {
    float vpp;    // Volt Picco-Picco (in LSB o mV)
    float vavg;   // Valore Medio
    float vrms;   // True RMS
    float vdc;    // Componente DC
    float freq;   // Frequenza in Hz
    uint8_t active;
    uint8_t f_active;
    uint8_t source;
    uint8_t type;
} ScopeMeasures;



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

// Stati possibili per l'encoder del trigger
typedef enum {
    ENC_MODE_TRIGGER_LEVEL,
    ENC_MODE_HYSTERESIS
} EncoderMode;

typedef enum { CUR_OFF = 0, CUR_VOLT, CUR_TIME } CursorType;

extern const char* time_base_labels[];
extern const uint32_t dds_table[];

// --- Configurazione Canali ---
extern float ch1_vdiv;
extern int16_t view_offset;
extern int16_t prev_view_offset;
extern bool pan_flag;
extern Point_t old_a;
extern Point_t old_b;
extern Point_t old_c;
extern Channel ch1;
extern Channel ch2;
// Variabili di stato cursori
extern CursorType cursor_type;
extern uint8_t cursor_source; // 1 = CH1, 2 = CH2
extern uint8_t cursor_select; // 0 = A, 1 = B, 2 = Entrambi

// Posizioni in pixel (0-400 o 0-240 a seconda dell'asse)
extern int16_t cursor_v_a, cursor_v_b; // Per Tensione
extern int16_t cursor_h_a, cursor_h_b; // Per Tempo

extern const float timebase_seconds[];
extern uint8_t current_time_base_idx;

extern char str_ta[];
extern char str_tb[];

extern ScopeMeasures *misure;
extern uint8_t old_meas_active;
extern uint8_t old_meas_type;
extern uint8_t old_meas_source;
extern uint8_t old_current_time_base_idx;
extern uint16_t old_trigger_level_12bit;
extern uint16_t trigger_level_12bit;
extern float old_freq;
extern uint8_t old_f_active;
extern bool is_running;
extern trigger_mode_t trigger_mode;
extern trig_slope_t trigger_slope;
extern bool freeze;
extern uint8_t currentMenu; // Default
extern uint8_t oldMenu;
extern EncoderMode current_enc_mode;
extern uint8_t trigger_hysteresis;
extern uint8_t trigger_source;
extern uint16_t *ch1_buffer;
extern uint16_t *ch2_buffer;
extern const float v_div_values[];
extern uint16_t *ch1_buffer;
extern uint16_t *ch2_buffer;
extern int16_t *old_buffer_a;
extern int16_t *old_buffer_b;





#endif