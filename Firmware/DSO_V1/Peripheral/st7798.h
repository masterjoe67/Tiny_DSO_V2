
#include <stddef.h>

#ifndef ST7798_DRIVER_H
#define ST7798_DRIVER_H

#define LOAD_FONT2
#define LOAD_GLCD

//Definiamo i registri basandoci sulla tua mappatura
#define TFT_DATA   _SFR_IO8(0x0D)  //(*(volatile uint8_t *)(0x0D + 0x20)) // SPDR (Offset I/O + 0x20)
#define TFT_STATUS _SFR_IO8(0x0E)  //(*volatile uint8_t *)(0x0E + 0x20)) // SPSR
#define TFT_CTRL   _SFR_IO8(0x0F)  //(*(volatile uint8_t *)(0x0F + 0x20)) // SPCR

// Bit del registro di controllo (SPCR)
#define TFT_DC_BIT   0
#define TFT_RST_BIT  1

#define ST_DELAY_BIT 0x80
#define ST77XX_SWRESET 0x01
#define ST77XX_MADCTL  0x36
#define ST77XX_COLMOD  0x3A
#define ST77XX_SLPOUT  0x11
#define ST77XX_DISPON  0x29

/***************************************************************************************
** ST7796S MADCTL Bit Definitions
***************************************************************************************/
#define MADCTL_MY  0x80  // Row Address Order (Inverte l'asse Y)
#define MADCTL_MX  0x40  // Column Address Order (Inverte l'asse X)
#define MADCTL_MV  0x20  // Row/Column Exchange (Scambia X e Y - per Landscape)
#define MADCTL_ML  0x10  // Vertical Refresh Order (Scansione LCD dall'alto/basso)
#define MADCTL_RGB 0x00  // Red-Green-Blue pixel order
#define MADCTL_BGR 0x08  // Blue-Green-Red pixel order (Il tuo display usa quasi certamente questo)
#define MADCTL_MH  0x04  // Horizontal Refresh Order (Scansione LCD da destra/sinistra)

// Modalità di rotazione per tft_setRotation()
#define PORTRAIT           0
#define LANDSCAPE          1
#define PORTRAIT_REVERSED  2
#define LANDSCAPE_REVERSED 3

// Colori Base
#define BLACK       0x0000
#define NAVY        0x000F
#define DARKGREEN   0x03E0
#define DARKCYAN    0x03EF
#define MAROON      0x7800
#define PURPLE      0x780F
#define OLIVE       0x7BE0
#define LIGHTGREY   0xC618
#define DARKGREY    0x7BEF

// Colori Brillanti (Ideali per tracce DSO)
#define BLUE        0x001F
#define GREEN       0x07E0
#define CYAN        0x07FF
#define RED         0xF800
#define MAGENTA     0xF81F
#define YELLOW      0xFFE0
#define WHITE       0xFFFF
#define ORANGE      0xFD20
#define GREENYELLOW 0xAFE5
#define PINK        0xF81F

// Sfumature di Grigio
#define DARKGREY    0x3186  // Grigio molto scuro (per sfondi barre)
#define GREY        0x8410  // Grigio medio (per la griglia)
#define LIGHTGREY   0xC618  // Grigio chiaro (per testi disattivi)

#ifdef LOAD_GLCD
  #include "glcdfont.c"
#endif

uint16_t textcolor, textbgcolor, fontsloaded, addr_row, addr_col;
int16_t  cursor_x, cursor_y, win_xe, win_ye, padX;
uint8_t  textfont, textsize, textdatum, rotation;

typedef struct {
	const unsigned char *chartbl;
	const unsigned char *widthtbl;
	unsigned       char height;
	} fontinfo;

typedef struct {
    int16_t x;
    int16_t y;
} Point_t;

void tft_init();
void tft_setRotation(uint8_t m);
static inline void tft_data16(uint16_t color);
void setTextColor(uint16_t c, uint16_t b);
void setTextSize(uint8_t s);
void setTextFont(uint8_t f);
void tft_set_cursor(int x, int y);
int16_t textWidth(const char *string, int font);
void tft_drawPixel(uint16_t x, uint16_t y, uint16_t color);
void tft_drawFastHLine(uint16_t x, uint16_t y, uint16_t w, uint16_t color);
void tft_drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color);
void tft_fillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void tft_fillCircle(uint16_t x0, uint16_t y0, uint16_t r, uint16_t color);
void tft_fillScreen(uint16_t color);
void tft_drawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color);
void tft_drawCircleHelper( int16_t x0, int16_t y0, int16_t r, uint8_t cornername, uint16_t color);
void tft_FillTriangle(Point_t p0, Point_t p1, Point_t p2, uint16_t color);
void tft_Print(const char *str);
size_t tft_write(uint8_t uniCode);
int tft_drawChar(unsigned int uniCode, int x, int y, int font);
void tft_drawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);
void tft_drawCharGL(int16_t x, int16_t y, unsigned char c, uint16_t color, uint16_t bg, uint8_t size);
void tft_drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
void tft_printAt(const char *str, int16_t x, int16_t y, uint16_t color, uint16_t bg);
#endif