#include <avr/pgmspace.h>
#include <stdbool.h>

#ifndef ILI9341_DRIVER_H
#define ILI9341_DRIVER_H

// ILI9341 -------------------------------------
#define CS_LOW()   (PORTB &= ~(1<<PB0))
#define CS_HIGH()  (PORTB |=  (1<<PB0))
#define DC_CMD()   (PORTB &= ~(1<<PB2))
#define DC_DATA()  (PORTB |=  (1<<PB2))
#define RST_LOW()  (PORTB &= ~(1<<PB1))
#define RST_HIGH() (PORTB |=  (1<<PB1))

// --- Definizioni colori RGB565 stile ILI9341 ---
#define ILI9341_BLACK       0x0000
#define ILI9341_NAVY        0x000F
#define ILI9341_DARKGREEN   0x03E0
#define ILI9341_DARKCYAN    0x03EF
#define ILI9341_MAROON      0x7800
#define ILI9341_PURPLE      0x780F
#define ILI9341_OLIVE       0x7BE0
#define ILI9341_LIGHTGREY   0xC618
#define ILI9341_DARKGREY    0x7BEF
#define ILI9341_BLUE        0x001F
#define ILI9341_GREEN       0x07E0
#define ILI9341_CYAN        0x07FF
#define ILI9341_RED         0xF800
#define ILI9341_MAGENTA     0xF81F
#define ILI9341_YELLOW      0xFFE0
#define ILI9341_WHITE       0xFFFF
#define ILI9341_ORANGE      0xFD20
#define ILI9341_GREENYELLOW 0xAFE5
#define ILI9341_PINK        0xF81F    // variazione magenta/rosa

#define ILI9341_CASET   0x2A
#define ILI9341_PASET   0x2B
#define ILI9341_RAMWR   0x2C

//These enumerate the text plotting alignment (reference datum point)
#define TL_DATUM 0 // Top left (default)
#define TC_DATUM 1 // Top centre
#define TR_DATUM 2 // Top right
#define ML_DATUM 3 // Middle left
#define CL_DATUM 3 // Centre left, same as above
#define MC_DATUM 4 // Middle centre
#define CC_DATUM 4 // Centre centre, same as above
#define MR_DATUM 5 // Middle right
#define CR_DATUM 5 // Centre right, same as above
#define BL_DATUM 6 // Bottom left
#define BC_DATUM 7 // Bottom centre
#define BR_DATUM 8 // Bottom right


#define LOAD_FONT2
#define LOAD_GLCD

#ifdef LOAD_GLCD
  #include "glcdfont.c"
#endif


typedef struct {
	const unsigned char *chartbl;
	const unsigned char *widthtbl;
	unsigned       char height;
	} fontinfo;

typedef struct {
    uint8_t count;
    uint16_t color;
} rle16_t;


typedef struct {
    uint8_t count;
    uint8_t color;   // 0 = nero, 1 = bianco
} rle_bw_t;
typedef uint8_t byte;

typedef struct {
    int16_t x;
    int16_t y;
} Point_t;

int16_t  cursor_x, cursor_y, win_xe, win_ye, padX;
uint8_t  textfont,
           textsize,
           textdatum,
           rotation;

uint16_t textcolor, textbgcolor, fontsloaded, addr_row, addr_col;
//bool  textwrap; // If set, 'wrap' text at right edge of display

extern unsigned int X_SIZE;
extern unsigned int Y_SIZE;

void spi_init(void);
void ILI9341_Init(void);
void ILI9341_Set_Rotation(unsigned char rotation);
void ILI9341_Set_Address(uint16_t X1, uint16_t Y1, uint16_t X2, uint16_t Y2);
void ILI9341_Fill_Screen(unsigned int color);
void ILI9341_Draw_Pixel(int x, int y, unsigned int color);
void ILI9341_Draw_Line(unsigned int color, unsigned int x1, unsigned int y1, unsigned int x2, unsigned int y2);
void ILI9341_Draw_Filled_Rectangle(unsigned int color,unsigned int x1, unsigned int y1, unsigned int x2, unsigned int y2);
void ILI9341_Draw_Empty_Rectangle(unsigned int color,unsigned int x1, unsigned int y1, unsigned int x2, unsigned int y2);
void ILI9341_Draw_Circle(unsigned int x0, unsigned int y0, int r, unsigned int color, unsigned char flood);
//void ILI9341_Draw_Char(int x, int y, uint16_t color, uint16_t bgcolor, uint8_t charcode, uint8_t size);
void ILI9341_set_cursor(int x, int y);
void ILI9341_set_text_color(uint16_t color, uint16_t bgcolor);
void ILI9341_set_text_size(uint8_t size);
//void ILI9341_putc(char c);
//void ILI9341_print(const char *str);
//void ILI9341_PrintInt(int val);
//void ILI9341_PrintPercent(int val);
//void ILI9341_PrintHz(int val);
//void ILI9341_PrintKHz(float val);
void ILI9341_FillTriangle(Point_t p0, Point_t p1, Point_t p2, uint16_t color);
void drawPixel(uint16_t x, uint16_t y, uint16_t color);

void fillScreen(uint16_t color);
void setTextColor(uint16_t c, uint16_t b);
void setTextSize(uint8_t s);

size_t ILI9341_write(uint8_t uniCode);
void ILI9341_Print(const char *str);

int drawChar(unsigned int uniCode, int x, int y, int font);
int drawString(const char *string, int poX, int poY, int font);
void drawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color);
void fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color);
void fillCircleHelper(int16_t x0, int16_t y0, int16_t r, uint8_t cornername, int16_t delta, uint16_t color);
void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color);
void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color);
void drawCircleHelper( int16_t x0, int16_t y0, int16_t r, uint8_t cornername, uint16_t color);
void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
void setTextFont(uint8_t f);
void spiWrite16(uint16_t data, int16_t count);
//void ILI9341_Draw_String(unsigned int x, unsigned int y, unsigned int color, unsigned int phone, char *str, unsigned char size);
uint16_t ILI9341_RGB565(uint8_t r, uint8_t g, uint8_t b);

uint8_t u16_to_decstr(uint16_t v, char *buf);
uint8_t u32_to_decstr(uint32_t v, char *buf);

//void ILI9341_draw_rle(const struct {uint8_t count; uint16_t color;} *rle, uint16_t x0, uint16_t y0, uint16_t width);
void ILI9341_draw_rle(const rle16_t *rle, uint16_t x0, uint16_t y0, uint16_t width);
void draw_rle_bw(uint16_t x0, uint16_t y0, uint16_t width, uint16_t height, const rle_bw_t *rle);

#endif