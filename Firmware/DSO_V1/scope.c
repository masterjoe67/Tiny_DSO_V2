#include <stdint.h>
#include <util/delay.h>
#include <stdbool.h>
#include "ili9341.h"
#include "scope.h"
#include "Peripheral/input.h"

// offset verticale per le tre tracce
#define CH0_Y  60
#define CH1_Y  120
#define CH2_Y  180

#define PRE_TRIGGER       150
#define POST_TRIGGER      150
#define BUFFER_TOTAL      (PRE_TRIGGER + POST_TRIGGER)

// Parametri reticolo
#define GRID_SPACING 30     // distanza tra linee
#define DOT_SPACING 4       // distanza tra puntini
#define COLOR_GRID ILI9341_WHITE

uint8_t buffer_a[BUFFER_TOTAL];
uint8_t buffer_b[BUFFER_TOTAL];
uint8_t buffer_c[BUFFER_TOTAL];
uint8_t old_buffer_a[300];
uint8_t old_buffer_b[300];
uint8_t old_buffer_c[300];

uint8_t time_div_sel = 10;
uint8_t prev_time_div_sel = 0xFF; // valore precedente (inesistente all'inizio)
int16_t view_offset = 0;
int16_t prev_view_offset = 0xFFFF;
int16_t prev_det_sig = 0;
bool freeze = false;
bool pan_flag = false;
bool time_div_sel_changed = true;

static trigger_mode_t trigger_mode = TRIG_MODE_AUTO;
static trig_slope_t trigger_slope = TRIG_SLOPE_RISING;
static tdiv_pan_t   mode_tdiv_pan = T_DIV;

static const char *time_div_str[20] = {
    "1uS",
    "2uS",
    "5uS",
    "10uS",
    "20uS",
    "50uS",
    "100uS",
    "200uS",
    "500uS",
    "1mS",
    "2mS",
    "5mS",
    "10mS",
    "20mS",
    "50mS",
    "100mS",
    "200mS",
    "500mS",
    "1S",
    "2S"
};

void set_base_time(uint8_t sel)
{
    REG_BASETIME = sel;   // 0..19
}

void set_trigger_level(uint16_t level12)
{
    level12 &= 0x0FFF;   // sicurezza
    uint8_t b0 = (uint8_t)(level12 & 0xFF);
    uint8_t b1 = (uint8_t)((level12 >> 8) & 0x03);

    REG_TRIGGER_LEVEL = b0;
    REG_TRIGGER_LEVEL = b1;
}

void draw_trig_mode_btn(){
    setTextSize(1);
    setTextColor(ILI9341_WHITE, 0x0000);
    switch (trigger_mode) {
        case TRIG_MODE_SINGLE:
            ILI9341_set_cursor(270, 24);
            ILI9341_Print("SING");
            break;

        case TRIG_MODE_NORMAL:
            ILI9341_set_cursor(270, 24);
            ILI9341_Print("NORM");
            break;

        case TRIG_MODE_AUTO:
            ILI9341_set_cursor(270, 24);
            ILI9341_Print("AUTO");
            break;
    }
}

void draw_trig_slope_btn(){
    fillRect(266, 57, 42, 42, 0x0000 );
    switch (trigger_slope) {
        case TRIG_SLOPE_RISING:
            
            ILI9341_Draw_Line(ILI9341_CYAN, 276, 95, 286, 75);
            ILI9341_Draw_Line(ILI9341_CYAN, 286, 75, 300, 75);
            break;

        case TRIG_SLOPE_FALLING:
            ILI9341_Draw_Line(ILI9341_CYAN, 276, 75, 286, 75);
            ILI9341_Draw_Line(ILI9341_CYAN, 286, 75, 300, 95);

            break;

    }
}

void draw_pan_tdiv_btn(){
    fillRect(266, 165, 42, 42, 0x0000 );
    switch (mode_tdiv_pan) {
        case T_DIV:
            ILI9341_set_cursor(270, 190);
            ILI9341_Print("T/Div");
            break;

        case PAN:
            ILI9341_set_cursor(270, 190);
            ILI9341_Print("PAN ");

            break;

    }
}

/* controlla se il core è pronto */
static inline bool osc_is_ready(void)
{
    return (REG_TRIG & 0x02) != 0;  // bit READY
}

void set_trigger_mode(trigger_mode_t mode, trig_slope_t slope)
{
    uint8_t v = 0;

    v |= (mode & 0x3) << 6;        // bits 7..6 = mode
    v |= (slope & 1) << 3;        // bit 3 = edge
    v |= (1 << 2);                // trig_enable = 1
    v |= (0 << 0);                // rearm = 0

    REG_CHC = v;
    trigger_mode = mode;
    trigger_slope = slope;
    draw_trig_mode_btn();
    draw_trig_slope_btn();
}

void osc_init_trigger(uint16_t trig_level, trigger_mode_t mode,
                      trig_channel_t chan, uint8_t edge_rising) {
    // Livello trigger 10 bit
    set_trigger_level(trig_level);

    // Modalità e canale trigger
    REG_TRIG = ((mode & 0x03) << 6) |      // bit 7-6: mode
               ((chan & 0x03) << 4) |      // bit 5-4: channel
               ((edge_rising?0:1) << 3) |  // bit3: edge (0=rising,1=falling)
               (1 << 2) |                  // bit2: trigger enable
               (1 << 0);                   // bit0: rearm
}


// funzione per disegnare la traccia sul TFT
void draw_trace(uint8_t *buffer, uint8_t *old_buffer, uint16_t length, uint16_t y_offset, uint16_t color)
{
    for (uint16_t i=10; i<length; i++) {
        // x ciclico su display
        uint16_t x = i - 8;

        uint8_t y = (buffer[i] / 2) + y_offset;  
        if (y >= Y_SIZE) y = Y_SIZE-1;
  
        drawPixel(x, old_buffer[i], 0x0000);
        drawPixel(x, y, color);
        old_buffer[i] = y;
    
   } 
}

void osc_wait_ready(void)
{
    // Bit READY già implementato in lettura da REG_TRIG o bit dedicato
    while (!(REG_TRIG & (1 << READY_BIT))) {
        // attesa attiva finché READY non diventa 1
        
    }
}


void rearm(){
    if (trigger_mode == TRIG_MODE_SINGLE & freeze){
        REG_TRIG = 0x01;
        freeze = false;
    }
}

void ToggleTriggerMode(void)
{
    switch (trigger_mode) {
        case TRIG_MODE_SINGLE:
            trigger_mode = TRIG_MODE_NORMAL;
            mode_tdiv_pan = T_DIV;
            break;

        case TRIG_MODE_NORMAL:
            trigger_mode = TRIG_MODE_AUTO;
            mode_tdiv_pan = T_DIV;
            break;

        case TRIG_MODE_AUTO:
        default:
            trigger_mode = TRIG_MODE_SINGLE;
            break;
    }

    set_trigger_mode(trigger_mode, trigger_slope);
    draw_pan_tdiv_btn();
    

}

void ToggleTriggerSlope(void)
{
    switch (trigger_slope) {
        case TRIG_SLOPE_RISING:
            trigger_slope = TRIG_SLOPE_FALLING;
            break;

        case TRIG_SLOPE_FALLING:
        default:
            trigger_slope = TRIG_SLOPE_RISING;
            break;

    }

    set_trigger_mode(trigger_mode, trigger_slope);
}

void ToggleTDivPan(void)
{
    switch (mode_tdiv_pan) {
        case T_DIV:
            mode_tdiv_pan = PAN;
            break;

        case PAN:
        default:
            mode_tdiv_pan = T_DIV;
            break;

    }

    draw_pan_tdiv_btn();
}


static inline void osc_arm_readout(void)
{
    REG_INDEX = 0;
}

void draw_time_div()
{
    if (time_div_sel > 19)
        return;
    ILI9341_set_cursor(209, 215);
    
    if(time_div_sel_changed){
       fillRect(209, 215, 37, 16, 0x0000); 
       time_div_sel_changed = false;
    }
    ILI9341_Print(time_div_str[time_div_sel]);
    /*uart_print("time_div_sel ");    
    uart_print_hex(time_div_sel);
    uart_print("\r\n");*/
}

Point_t old_a = { 0, 0 };
Point_t old_b = { 0, 0 };
Point_t old_c = { 0, 0 };
void drawPanTrack(){

    int16_t offset = 125 + view_offset;

    Point_t a = { offset, 2 };
    Point_t b = { offset + 10, 2 };
    Point_t c = { offset + 5, 17 };
    
    if(pan_flag){
        ILI9341_FillTriangle(old_a, old_b, old_c, ILI9341_BLACK);
        old_a = a;
        old_b = b;
        old_c = c;
    }
    ILI9341_FillTriangle(a, b, c, ILI9341_WHITE);
}

void drawScreenValue(){
    draw_time_div();
    //drawPanTrack();
}


void drawDottedGridFast(int x0, int y0, int x1, int y1, int gridSpacing, int dotSpacing, uint16_t color) {

    drawPanTrack();

  // orizzontali puntinate
  for (int y = y0; y <= y1; y += gridSpacing) {
    for (int x = x0; x <= x1; x += dotSpacing) {
      drawPixel(x, y, color);
    }
  }

  // verticali puntinate
  for (int x = x0; x <= x1; x += gridSpacing) {
    for (int y = y0; y <= y1; y += dotSpacing) {
      drawPixel(x, y, color);
    }
  }

  drawScreenValue();
}




void osc_read_triggered(uint8_t *a, uint8_t *b, uint8_t *c)
{
    /* SINGLE: se già congelato, NON riarmare acquisizione */
    if (trigger_mode == TRIG_MODE_SINGLE && freeze) {
        osc_arm_readout();   // solo lettura memoria
    } else {
        osc_wait_ready();

        if (trigger_mode == TRIG_MODE_SINGLE) {
            freeze = true;
            prev_det_sig= encoder_read();
        }

        osc_arm_readout();   // acquisizione + lettura
    }

    for (int i = 0; i < 255; i++) {
        b[i] = REG_CHB;
        c[i] = REG_CHC;
        a[i] = REG_CHA;
    }

    /* riarmo trigger SOLO se non in HOLD */
    if (!(trigger_mode == TRIG_MODE_SINGLE)) {
        REG_TRIG = 0x01;
    }
}

int16_t update_view_offset(
    int16_t param,
    int16_t min,
    int16_t max,
    int16_t step
)
{
    int16_t det  = encoder_read();
    int16_t diff = det - prev_det_sig;
    prev_det_sig = det;

    /* nessun movimento */
    if (diff == 0)
        return param;

    /* filtro solo per glitch grossi (wrap / rumore) */
    if (diff > 20 || diff < -20)
        return param;

    /* calcolo SEMPRE in 32 bit */
    int32_t tmp = (int32_t)param + (int32_t)diff * (int32_t)step;

    /* clamp corretto */
    if (tmp < min) tmp = min;
    if (tmp > max) tmp = max;

    return (int16_t)tmp;
}



void oscilloscope_init(void)
{
    fillScreen(0x0000);
    setTextSize(1);
    setTextColor(ILI9341_WHITE, 0x0000);
    drawRoundRect(0, 1, 255, 239, 6, ILI9341_WHITE);
    drawRoundRect(263, 1, 48, 48, 6, ILI9341_YELLOW);
    drawRoundRect(263, 54, 48, 48, 6, ILI9341_YELLOW);
    drawRoundRect(263, 108, 48, 48, 6, ILI9341_YELLOW);
    drawRoundRect(263, 162, 48, 48, 6, ILI9341_YELLOW);
    drawRoundRect(263, 216, 48, 24, 6, ILI9341_YELLOW);
    drawDottedGridFast(8, 0, 254, 238, 40, 4, ILI9341_WHITE);
    draw_trig_mode_btn();
    draw_pan_tdiv_btn();
}

static inline void osc_write_view_offset(int16_t offset)
{
    REG_BASETIME = 0xFF;                         // escape
    REG_BASETIME = 0x01;                         // comando: view_offset
    REG_BASETIME = (uint8_t)(offset & 0xFF);     // LSB
    REG_BASETIME = (uint8_t)(offset >> 8);       // MSB
    REG_BASETIME = 0xFF;

}


void acquire_and_draw(){
    drawDottedGridFast(8, 0, 254, 238, 40, 4, ILI9341_WHITE);
    osc_read_triggered(buffer_a, buffer_b, buffer_c);
    draw_trace(buffer_a, old_buffer_a, 255, CH0_Y, ILI9341_GREEN);
    draw_trace(buffer_b, old_buffer_b, 255, CH0_Y, ILI9341_RED);
    draw_trace(buffer_c, old_buffer_c, 255, CH0_Y, ILI9341_BLUE);
}

/************************************************************************************/
/*                                 KEY MAP                                          */
/*          ROW0            ROW1        ROW2                                            */
/*      12 Carrier       13 Mode             */
/*       9 Modul         10 Output      */
/*       6 Magnit         7 STEP              */
/*       3 Dead                         */
/*       0 Enter                        */
/********************************************************************************** */
// --- main loop ---
void scope_main(void)
{
    uint8_t key, rep;
    oscilloscope_init();
    set_base_time(12);
    set_trigger_level(100);   // metà scala
    set_trigger_mode(TRIG_MODE_AUTO, TRIG_SLOPE_RISING);
    while(1)
    {
        pan_flag = false;

        uint8_t ev = 0xff;


        if (keypad_poll(&key, &rep)) {
            //ui_handle_key(key, rep);
            ev = key;

        } else {
            ev = 0xFF;
        }
        
        
        if(ev != 0xFF){
            switch (ev)
            {
            case 12:
                ToggleTriggerMode();
                break;
            case 9:
                ToggleTriggerSlope();
                break;
            case 6:
                rearm();
                break;
            case 3:
                //Pan - T/Div
                ToggleTDivPan();
   
                break;
   
            default:
                break;
            }
        }

uint8_t new_sel;
int16_t new_pan;
    switch (mode_tdiv_pan) {
        case T_DIV:
            new_sel = update_param_8(time_div_sel, 0, 16, 1);
            if (new_sel != prev_time_div_sel) {
                // Aggiorna il registro solo se il valore è cambiato
                REG_BASETIME = new_sel;
                prev_time_div_sel = new_sel;
                time_div_sel = new_sel; // aggiorna il valore corrente
                time_div_sel_changed = true;
            }
            break;

        case PAN:
            new_pan = update_view_offset(view_offset , -PAN_LIMIT, +PAN_LIMIT, PAN_STEP);
            if (new_pan != prev_view_offset) {
                
                // Aggiorna il registro solo se il valore è cambiato
                osc_write_view_offset(new_pan);
                osc_arm_readout(); 
                prev_view_offset = new_pan;
                view_offset = new_pan; // aggiorna il valore corrente
                pan_flag = true;
                /*uart_print("offset ");
                uart_print_int16(view_offset);
                uart_print("\r\n");*/
            }
            break;
        default:
            
            break;

    }
    acquire_and_draw();
  

        
        /*uint8_t new_sel = update_param_8(time_div_sel, 0, 16, 1);
        if (new_sel != prev_time_div_sel) {
            // Aggiorna il registro solo se il valore è cambiato
            REG_BASETIME = new_sel;
            prev_time_div_sel = new_sel;
            time_div_sel = new_sel; // aggiorna il valore corrente
        }
        if(z == 1) return;   // 1 = tocco valido
        
        if(z == 0){
            ToggleTriggerMode();
        }*/
        //_delay_ms(50);
        

    }
}
