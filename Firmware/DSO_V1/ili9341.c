
//#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <avr/pgmspace.h>

#include"ili9341.h"
//#include "characters.h"
#include "Font16.h"

//#include "glcdfont.c"

unsigned int X_SIZE = 320;
unsigned int Y_SIZE = 240;

//static int cursor_x = 0;
//static int cursor_y = 0;
static uint16_t text_color = 0xFFFF;   // default bianco
static uint16_t text_bg = 0x0000;      // default nero
static uint8_t text_size = 1;

bool  textwrap = true;

unsigned char hh;

const PROGMEM fontinfo fontdata [] = {
   { 0, 0, 0 },

   { 0, 0, 8 },

  #ifdef LOAD_FONT2
   { (const unsigned char *)chrtbl_f16, widtbl_f16, chr_hgt_f16},
  #else
   { 0, 0, 0 },
  #endif

   { 0, 0, 0 },

  #ifdef LOAD_FONT4
   { (const unsigned char *)chrtbl_f32, widtbl_f32, chr_hgt_f32},
  #else
   { 0, 0, 0 },
  #endif

   { 0, 0, 0 },

  #ifdef LOAD_FONT6
   { (const unsigned char *)chrtbl_f64, widtbl_f64, chr_hgt_f64},
  #else
   { 0, 0, 0 },
  #endif

  #ifdef LOAD_FONT7
   { (const unsigned char *)chrtbl_f7s, widtbl_f7s, chr_hgt_f7s},
  #else
   { 0, 0, 0 },
  #endif

  #ifdef LOAD_FONT8
   { (const unsigned char *)chrtbl_f72, widtbl_f72, chr_hgt_f72}
  #else
   { 0, 0, 0 }
  #endif
};

#if defined(__GNUC__)
    // Versione migliore: usa typeof() e mantiene il tipo esatto
    #define swap(a, b)  do { typeof(a) _t = (a); (a) = (b); (b) = _t; } while(0)
#else
    // Versione compatibile, usa int
    #define swap(a, b)  do { int _t = (a); (a) = (b); (b) = _t; } while(0)
#endif

static void swap_int16(int16_t *a, int16_t *b)
{
    int16_t t = *a;
    *a = *b;
    *b = t;
}


void spi_init(void) {
    // MOSI (PB2), SCK (PB1), SS (PB4) come output
    DDRB |= (1<<PB2)|(1<<PB1)|(1<<PB4);

    // SPI: Master, clk/2, mode 0
    SPCR = (1<<SPE)|(1<<MSTR);
    SPSR = (1<<SPI2X);
}

/***************************************************************************************
** Function name:           spiWrite16
** Descriptions:            Delay based assembler loop for fast SPI write
***************************************************************************************/
void spiWrite16(uint16_t data, int16_t count)
{
	if (count <= 0) return;

    uint8_t hi = data >> 8;
    uint8_t lo = data & 0xFF;

	DC_DATA();   // dati
    CS_LOW();    // seleziona display

    while (count--)
    {
        // --- invio MSB ---
        SPDR = hi;
        while (!(SPSR & _BV(SPIF)));   // attende fine trasmissione

        // (eventuale piccolo delay per imitare i cicli dell'assembly)
        __asm__ __volatile__("nop\nnop\nnop\n");

        // --- invio LSB ---
        SPDR = lo;
        while (!(SPSR & _BV(SPIF)));   // attende fine trasmissione

        // altro leggero delay, se necessario
        __asm__ __volatile__("nop\nnop\n");
    }
	CS_HIGH();
}


void spi_write(uint8_t v) {
    SPDR = v;
    while(!(SPSR & (1<<SPIF)));
}

static inline void spi_send16(uint16_t v)
{
   DC_DATA();   // dati
    CS_LOW();    // seleziona display
    spi_write(v >> 8);
    spi_write(v & 0xFF);
}

void ili9341_sendCmd(uint8_t cmd) {
    DC_CMD();
    CS_LOW();
    spi_write(cmd);
    CS_HIGH();
}

void ILI9341_Send_Data(uint8_t d) {
    DC_DATA();
    CS_LOW();
    spi_write(d);
    CS_HIGH();
}

void ILI9341_Reset(void) {
    //leds(1);
    RST_LOW();
    _delay_ms(20);
    RST_HIGH();
    _delay_ms(150);
    //leds(2);
}

void ILI9341_Send_Burst(uint16_t color, uint32_t repetitions)
{


    // Invio ripetuto: ottimizzato
    while (repetitions--)
    {
        spi_send16(color);
    }

    CS_HIGH();
}

void ILI9341_Draw_Filled_Rectangle(unsigned int color,unsigned int x1, unsigned int y1, unsigned int x2, unsigned int y2)
{
	ILI9341_Set_Address(x1, y1, x2, y2);
	ILI9341_Send_Burst(color, (uint16_t)((long)(x2-x1+1) * (long)(y2-y1+1)));
}

void ILI9341_Draw_Empty_Rectangle(unsigned int color,unsigned int x1, unsigned int y1, unsigned int x2, unsigned int y2)
{
	ILI9341_Draw_Line(color, x1, y1, x2, y1);
	ILI9341_Draw_Line(color, x2, y1, x2, y2);
	ILI9341_Draw_Line(color, x1, y1, x1, y2);
	ILI9341_Draw_Line(color, x1, y2, x2, y2);
}

void ILI9341_Draw_Line(unsigned int color, unsigned int x1, unsigned int y1, unsigned int x2, unsigned int y2)
{
	int steep = abs(y2-y1) > abs(x2-x1);

	if (steep)
	{
		swap(x1,y1);
		swap(x2,y2);
	}

	if(x1>x2)
	{
		swap(x1,x2);
		swap(y1,y2);
	}

	int dx,dy;
	dx = (x2 - x1);
	dy = abs(y2 - y1);
	int err = dx / 2;
	int ystep;
	if(y1 < y2)
	{
		ystep = 1;
	}
	else
	{
		ystep = -1;
	}
	for (; x1 <= x2; x1++)
	{
		if (steep)
		{
			drawPixel(y1, x1, color);
		}
		else
		{
			drawPixel(x1, y1, color);
		}
		err -= dy;
		if (err < 0)
		{
			y1 += ystep;
			err += dx;
		}
	}
}

void ILI9341_Init() {
	//ili9341_spi_init();
	//SPI_Cmd(SPI2, ENABLE);
	/* Reset The Screen */
	ILI9341_Reset();
	ili9341_sendCmd(0x01);
	_delay_ms(500);

	/* Power Control A */
	ili9341_sendCmd(0xCB);
	ILI9341_Send_Data(0x39);
	ILI9341_Send_Data(0x2C);
	ILI9341_Send_Data(0x00);
	ILI9341_Send_Data(0x34);
	ILI9341_Send_Data(0x02);

	/* Power Control B */
	ili9341_sendCmd(0xCF);
	ILI9341_Send_Data(0x00);
	ILI9341_Send_Data(0xC1);
	ILI9341_Send_Data(0x30);

	/* Driver timing control A */
	ili9341_sendCmd(0xE8);
	ILI9341_Send_Data(0x85);
	ILI9341_Send_Data(0x00);
	ILI9341_Send_Data(0x78);

	/* Driver timing control B */
	ili9341_sendCmd(0xEA);
	ILI9341_Send_Data(0x00);
	ILI9341_Send_Data(0x00);

	/* Power on Sequence control */
	ili9341_sendCmd(0xED);
	ILI9341_Send_Data(0x64);
	ILI9341_Send_Data(0x03);
	ILI9341_Send_Data(0x12);
	ILI9341_Send_Data(0x81);

	/* Pump ratio control */
	ili9341_sendCmd(0xF7);
	ILI9341_Send_Data(0x20);

	/* Power Control 1 */
	ili9341_sendCmd(0xC0);
	ILI9341_Send_Data(0x23);

	/* Power Control 2 */
	ili9341_sendCmd(0xC1);
	ILI9341_Send_Data(0x10);

	/* VCOM Control 1 */
	ili9341_sendCmd(0xC5);
	ILI9341_Send_Data(0x3E);
	ILI9341_Send_Data(0x28);

	/* VCOM Control 2 */
	ili9341_sendCmd(0xC7);
	ILI9341_Send_Data(0x86);

	/* VCOM Control 2 */
	ili9341_sendCmd(0x36);
	ILI9341_Send_Data(0x48);

	/* Pixel Format Set */
	ili9341_sendCmd(0x3A);
	ILI9341_Send_Data(0x55);    //16bit

	ili9341_sendCmd(0xB1);
	ILI9341_Send_Data(0x00);
	ILI9341_Send_Data(0x18);

	/* Display Function Control */
	ili9341_sendCmd(0xB6);
	ILI9341_Send_Data(0x08);
	ILI9341_Send_Data(0x82);
	ILI9341_Send_Data(0x27);

	/* 3GAMMA FUNCTION DISABLE */
	ili9341_sendCmd(0xF2);
	ILI9341_Send_Data(0x00);

	/* GAMMA CURVE SELECTED */
	ili9341_sendCmd(0x26);  //Gamma set
	ILI9341_Send_Data(0x01); 	//Gamma Curve (G2.2)

	//Positive Gamma  Correction
	ili9341_sendCmd(0xE0);
	ILI9341_Send_Data(0x0F);
	ILI9341_Send_Data(0x31);
	ILI9341_Send_Data(0x2B);
	ILI9341_Send_Data(0x0C);
	ILI9341_Send_Data(0x0E);
	ILI9341_Send_Data(0x08);
	ILI9341_Send_Data(0x4E);
	ILI9341_Send_Data(0xF1);
	ILI9341_Send_Data(0x37);
	ILI9341_Send_Data(0x07);
	ILI9341_Send_Data(0x10);
	ILI9341_Send_Data(0x03);
	ILI9341_Send_Data(0x0E);
	ILI9341_Send_Data(0x09);
	ILI9341_Send_Data(0x00);

	//Negative Gamma  Correction
	ili9341_sendCmd(0xE1);
	ILI9341_Send_Data(0x00);
	ILI9341_Send_Data(0x0E);
	ILI9341_Send_Data(0x14);
	ILI9341_Send_Data(0x03);
	ILI9341_Send_Data(0x11);
	ILI9341_Send_Data(0x07);
	ILI9341_Send_Data(0x31);
	ILI9341_Send_Data(0xC1);
	ILI9341_Send_Data(0x48);
	ILI9341_Send_Data(0x08);
	ILI9341_Send_Data(0x0F);
	ILI9341_Send_Data(0x0C);
	ILI9341_Send_Data(0x31);
	ILI9341_Send_Data(0x36);
	ILI9341_Send_Data(0x0F);

	//EXIT SLEEP
	ili9341_sendCmd(0x11);
	_delay_ms(120);
	//TURN ON DISPLAY
	ili9341_sendCmd(0x29);
	//ILI9341_Send_Data(0x2C);
	//SPI_Cmd(SPI2, DISABLE);
}

void ILI9341_Set_Rotation(unsigned char rotation) {
	ili9341_sendCmd(0x36);
	switch (rotation) {
	case 0:
		ILI9341_Send_Data(0x48);
		X_SIZE = 240;
		Y_SIZE = 320;
		break;
	case 1:
		ILI9341_Send_Data(0x28);
		X_SIZE = 320;
		Y_SIZE = 240;
		break;
	case 2:
		ILI9341_Send_Data(0x88);
		X_SIZE = 240;
		Y_SIZE = 320;
		break;
	case 3:
		ILI9341_Send_Data(0xE8);
		X_SIZE = 320;
		Y_SIZE = 240;
		break;
	}
}

void ILI9341_Set_Address(uint16_t X1, uint16_t Y1, uint16_t X2, uint16_t Y2)
{
ili9341_sendCmd(0x2A);
ILI9341_Send_Data(X1>>8);
ILI9341_Send_Data(X1);
ILI9341_Send_Data(X2>>8);
ILI9341_Send_Data(X2);

ili9341_sendCmd(0x2B);
ILI9341_Send_Data(Y1>>8);
ILI9341_Send_Data(Y1);
ILI9341_Send_Data(Y2>>8);
ILI9341_Send_Data(Y2);

ili9341_sendCmd(0x2C);
}

void ILI9341_Fill_Screen(unsigned int color)
{

	ILI9341_Set_Address(0, 0, X_SIZE-1, Y_SIZE-1);
	DC_DATA();
    CS_LOW();
	for(int16_t row = 0; row < Y_SIZE; row++)
    {
        for(int16_t col = 0; col < X_SIZE; col++)
        {
            ILI9341_Send_Data(color >> 8);
            ILI9341_Send_Data(color & 0xFF);
        }
    }
	CS_HIGH();
}

void ILI9341_Draw_Pixel(int x, int y, unsigned int color)
{
	if((x < 0) || (y < 0) || (x >= X_SIZE) || (y >= Y_SIZE))
	{
		return;
	}
	ILI9341_Set_Address(x, y, X_SIZE-1, Y_SIZE-1);
	DC_CMD();;
	ILI9341_Send_Data(0x2C);
	DC_DATA();
	ILI9341_Send_Data(color>>8);
	ILI9341_Send_Data(color);
}

void ILI9341_Draw_Circle(unsigned int x0, unsigned int y0, int r, unsigned int color, unsigned char flood) {
	int f = 1 - r;
	int ddF_x = 1;
	int ddF_y = -2 * r;
	int x = 0;
	int y = r;
	if (flood == 0) {
		ILI9341_Draw_Pixel(x0, y0 + r, color);
		ILI9341_Draw_Pixel(x0, y0 - r, color);
		ILI9341_Draw_Pixel(x0 + r, y0, color);
		ILI9341_Draw_Pixel(x0 - r, y0, color);
		while (x < y) {
			if (f >= 0) {
				y--;
				ddF_y += 2;
				f += ddF_y;
			}
			x++;
			ddF_x += 2;
			f += ddF_x;
			ILI9341_Draw_Pixel(x0 + x, y0 + y, color);
			ILI9341_Draw_Pixel(x0 - x, y0 + y, color);
			ILI9341_Draw_Pixel(x0 + x, y0 - y, color);
			ILI9341_Draw_Pixel(x0 - x, y0 - y, color);
			ILI9341_Draw_Pixel(x0 + y, y0 + x, color);
			ILI9341_Draw_Pixel(x0 - y, y0 + x, color);
			ILI9341_Draw_Pixel(x0 + y, y0 - x, color);
			ILI9341_Draw_Pixel(x0 - y, y0 - x, color);
		}
	} else {
		ILI9341_Draw_Pixel(x0, y0 + r, color);
		ILI9341_Draw_Pixel(x0, y0 - r, color);
		ILI9341_Set_Address(x0 - r, y0, x0 + r, y0);
		DC_CMD();
		spi_write(0x2C);
		DC_DATA();
		for (uint32_t fff = 0; fff < r * 2 + 1; fff++) {
			spi_write(color >> 8);
			spi_write(color);
		}
		while (x < y) {
			if (f >= 0) {
				y--;
				ddF_y += 2;
				f += ddF_y;
			}
			x++;
			ddF_x += 2;
			f += ddF_x;
			ILI9341_Set_Address(x0 - x, y0 + y, x0 + x, y0 + y);
			DC_CMD();
			spi_write(0x2C);
			DC_DATA();
			for (uint32_t fff = 0; fff < x * 2 + 1; fff++) {
				spi_write(color >> 8);
				spi_write(color);
			}
			ILI9341_Set_Address(x0 - x, y0 - y, x0 + x, y0 - y);
			DC_CMD();
			spi_write(0x2C);
			DC_DATA();
			for (uint32_t fff = 0; fff < x * 2 + 1; fff++) {
				spi_write(color >> 8);
				spi_write(color);
			}
			ILI9341_Set_Address(x0 - y, y0 + x, x0 + y, y0 + x);
			DC_CMD();
			spi_write(0x2C);
			DC_DATA();
			for (uint32_t fff = 0; fff < y * 2 + 1; fff++) {
				spi_write(color >> 8);
				spi_write(color);
			}
			ILI9341_Set_Address(x0 - y, y0 - x, x0 + y, y0 - x);
			DC_CMD();
			spi_write(0x2C);
			DC_DATA();
			for (uint32_t fff = 0; fff < y * 2 + 1; fff++) {
				spi_write(color >> 8);
				spi_write(color);
			}
		}
	}
}

/*void ILI9341_Draw_Char(int x, int y, uint16_t color, uint16_t bgcolor, uint8_t charcode, uint8_t size)
{
    if (charcode < 0x20 || charcode > 0x7F) return;

    const uint8_t *glyph = chars8[charcode - 0x20];

    int fw = 8;   // 8 colonne reali + 1 di spazio
    int fh = 8;

    int x2 = x + fw * size - 1;
    int y2 = y + fh * size - 1;

    ILI9341_Set_Address(x, y, x2, y2);

    DC_DATA();
    CS_LOW();
    uint8_t mask = 0x1;
    for (int row = 0; row < fh; row++)
    {
        uint8_t rowbyte = glyph[row];
 
        //uint8_t xss = 0;
        // scaling verticale
        for (int ys = 0; ys < size; ys++)
        {
            // --- 8 colonne reali del carattere ---
            for (int col = 0; col < 8; col++)
            {
                uint8_t bit = (rowbyte >> (7 - col)) & 1;
                
                for (int xs = 0; xs < size; xs++){
                    
                    if (bit) {
                        SPDR = color >> 8; asm volatile( "nop\n\t" ::); // Sync to SPIF bit
                        while (!(SPSR & _BV(SPIF)));
                        SPDR = color;
                        while (!(SPSR & _BV(SPIF)));
                    }
                    else {
                        SPDR = bgcolor >> 8; asm volatile( "nop\n\t" ::);    
                        while (!(SPSR & _BV(SPIF)));
                        SPDR = bgcolor;
                        while (!(SPSR & _BV(SPIF)));
                    
                    }
                }
            }

            // --- colonna di spaziatura ---
            //for (int xs = 0; xs < size; xs++) spi_send16(bgcolor);
        }
    }

    CS_HIGH();
}*/

void ILI9341_set_cursor(int x, int y) {
    cursor_x = x;
    cursor_y = y;
}

void ILI9341_set_text_color(uint16_t color, uint16_t bgcolor) {
    text_color = color;
    text_bg = bgcolor;
}

void ILI9341_set_text_size(uint8_t size) {
    text_size = size;
}



/***************************************************************************************
** Function name:           fillCircleHelper
** Description:             Support function for filled circle drawing
***************************************************************************************/
// Used to do circles and roundrects
void fillCircleHelper(int16_t x0, int16_t y0, int16_t r, uint8_t cornername, int16_t delta, uint16_t color)
{
  int16_t f     = 1 - r;
  int16_t ddF_x = 1;
  int16_t ddF_y = -r - r;
  int16_t x     = 0;

  delta++;
  while (x < r) {
    if (f >= 0) {
      r--;
      ddF_y += 2;
      f     += ddF_y;
    }
    x++;
    ddF_x += 2;
    f     += ddF_x;

    if (cornername & 0x1) {
      drawFastVLine(x0 + x, y0 - r, r + r + delta, color);
      drawFastVLine(x0 + r, y0 - x, x + x + delta, color);
    }
    if (cornername & 0x2) {
      drawFastVLine(x0 - x, y0 - r, r + r + delta, color);
      drawFastVLine(x0 - r, y0 - x, x + x + delta, color);
    }
  }
}

/***************************************************************************************
** Function name:           fillRoundRect
** Description:             Draw a rounded corner filled rectangle
***************************************************************************************/
// Fill a rounded rectangle
void fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color)
{
  // smarter version
  fillRect(x + r, y, w - r - r, h, color);

  // draw four corners
  fillCircleHelper(x + w - r - 1, y + r, r, 1, h - r - r - 1, color);
  fillCircleHelper(x + r    , y + r, r, 2, h - r - r - 1, color);
}

/***************************************************************************************
** Function name:           drawRoundRect
** Description:             Draw a rounded corner rectangle outline
***************************************************************************************/
void drawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color)
{
  // smarter version
  drawFastHLine(x + r  , y    , w - r - r, color); // Top
  drawFastHLine(x + r  , y + h - 1, w - r - r, color); // Bottom
  drawFastVLine(x    , y + r  , h - r - r, color); // Left
  drawFastVLine(x + w - 1, y + r  , h - r - r, color); // Right
  // draw four corners
  drawCircleHelper(x + r    , y + r    , r, 1, color);
  drawCircleHelper(x + w - r - 1, y + r    , r, 2, color);
  drawCircleHelper(x + w - r - 1, y + h - r - 1, r, 4, color);
  drawCircleHelper(x + r    , y + h - r - 1, r, 8, color);
}

/***************************************************************************************
** Function name:           drawFastVLine
** Description:             draw a vertical line
***************************************************************************************/
void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color)
{
#ifdef CLIP_CHECK
  // Rudimentary clipping
  if ((x >= _width) || (y >= _height)) return;
  if ((y + h - 1) >= _height) h = _height - y;
#endif

  

  ILI9341_Set_Address(x, y, x, y + h - 1);

  spiWrite16(color, h);
  CS_HIGH();

  
}

/***************************************************************************************
** Function name:           drawFastHLine
** Description:             draw a horizontal line
***************************************************************************************/
void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color)
{
#ifdef CLIP_CHECK
  // Rudimentary clipping
  if ((x >= _width) || (y >= _height)) return;
  if ((x + w - 1) >= _width)  w = _width - x;
#endif


  ILI9341_Set_Address(x, y, x + w - 1, y);

  spiWrite16(color, w);
  CS_HIGH();

  
}

/***************************************************************************************
** Function name:           drawPixel
** Description:             push a single pixel at an arbitrary position
***************************************************************************************/
void drawPixel(uint16_t x, uint16_t y, uint16_t color)
{
  // Faster range checking, possible because x and y are unsigned
  if ((x >= X_SIZE) || (y >= Y_SIZE)) return;
  //spi_begin();

  CS_LOW();

if (addr_col != x) {
  DC_CMD();
  SPDR = ILI9341_CASET;
  while (!(SPSR & _BV(SPIF)));   // attende fine trasmissione
  addr_col = x;
  DC_DATA();
  SPDR = x >> 8; while (!(SPSR & _BV(SPIF)));   // attende fine trasmissione
  SPDR = x; while (!(SPSR & _BV(SPIF)));   // attende fine trasmissione

  SPDR = x >> 8; while (!(SPSR & _BV(SPIF)));   // attende fine trasmissione
  SPDR = x; while (!(SPSR & _BV(SPIF)));   // attende fine trasmissione
}

if (addr_row != y) {
  DC_CMD();
  SPDR = ILI9341_PASET;
  while (!(SPSR & _BV(SPIF)));   // attende fine trasmissione
  addr_row = y;
  DC_DATA();
  SPDR = y >> 8; while (!(SPSR & _BV(SPIF)));   // attende fine trasmissione
  SPDR = y; while (!(SPSR & _BV(SPIF)));   // attende fine trasmissione

  SPDR = y >> 8; while (!(SPSR & _BV(SPIF)));   // attende fine trasmissione
  SPDR = y; while (!(SPSR & _BV(SPIF)));   // attende fine trasmissione
}

  DC_CMD();

  SPDR = ILI9341_RAMWR; while (!(SPSR & _BV(SPIF)));   // attende fine trasmissione

  DC_DATA();

  SPDR = color >> 8; while (!(SPSR & _BV(SPIF)));   // attende fine trasmissione
  win_xe=x;
  SPDR = color; while (!(SPSR & _BV(SPIF)));   // attende fine trasmissione
  win_ye=y;

  CS_HIGH();

  
}

/***************************************************************************************
** Function name:           drawCircleHelper
** Description:             Support function for circle drawing
***************************************************************************************/
void drawCircleHelper( int16_t x0, int16_t y0, int16_t r, uint8_t cornername, uint16_t color)
{
  int16_t f     = 1 - r;
  int16_t ddF_x = 1;
  int16_t ddF_y = -2 * r;
  int16_t x     = 0;

  while (x < r) {
    if (f >= 0) {
      r--;
      ddF_y += 2;
      f     += ddF_y;
    }
    x++;
    ddF_x += 2;
    f     += ddF_x;
    if (cornername & 0x4) {
      drawPixel(x0 + x, y0 + r, color);
      drawPixel(x0 + r, y0 + x, color);
    }
    if (cornername & 0x2) {
      drawPixel(x0 + x, y0 - r, color);
      drawPixel(x0 + r, y0 - x, color);
    }
    if (cornername & 0x8) {
      drawPixel(x0 - r, y0 + x, color);
      drawPixel(x0 - x, y0 + r, color);
    }
    if (cornername & 0x1) {
      drawPixel(x0 - r, y0 - x, color);
      drawPixel(x0 - x, y0 - r, color);
    }
  }
}


/***************************************************************************************
** Function name:           fillRect
** Description:             draw a filled rectangle
***************************************************************************************/
void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
{

  // rudimentary clipping (drawChar w/big text requires this)
  if ((x > X_SIZE) || (y > Y_SIZE) || (w==0) || (h==0)) return;
  if ((x + w - 1) > X_SIZE)  w = X_SIZE  - x;
  if ((y + h - 1) > Y_SIZE) h = Y_SIZE - y;


  //spi_begin();
  ILI9341_Set_Address(x, y, x + w - 1, y + h - 1);

  while (h--) spiWrite16(color, w);
  CS_HIGH();
}

void ILI9341_FillTriangle(Point_t p0, Point_t p1, Point_t p2, uint16_t color)
{
    if (p0.y > p1.y) { swap_int16(&p0.y, &p1.y); swap_int16(&p0.x, &p1.x); }
    if (p1.y > p2.y) { swap_int16(&p1.y, &p2.y); swap_int16(&p1.x, &p2.x); }
    if (p0.y > p1.y) { swap_int16(&p0.y, &p1.y); swap_int16(&p0.x, &p1.x); }

    int16_t total_height = p2.y - p0.y;

    for (int16_t y = p0.y; y <= p2.y; y++) {
        uint8_t second_half = y > p1.y || p1.y == p0.y;
        int16_t segment_height = second_half ? p2.y - p1.y : p1.y - p0.y;

        float alpha = (float)(y - p0.y) / total_height;
        float beta  = (float)(y - (second_half ? p1.y : p0.y)) / segment_height;

        int16_t ax = p0.x + (p2.x - p0.x) * alpha;
        int16_t bx = second_half
            ? p1.x + (p2.x - p1.x) * beta
            : p0.x + (p1.x - p0.x) * beta;

        if (ax > bx) swap_int16(&ax, &bx);
        int16_t w = bx - ax;
        drawFastHLine(ax, y, w, color);
    }
}
/***************************************************************************************
** Function name:           fillScreen
** Description:             Clear the screen to defined colour
***************************************************************************************/
void fillScreen(uint16_t color)
{
  fillRect(0, 0, X_SIZE, Y_SIZE, color);
}

/***************************************************************************************
** Function name:           setTextColor
** Description:             Set the font foreground and background colour
***************************************************************************************/
void setTextColor(uint16_t c, uint16_t b)
{
  textcolor   = c;
  textbgcolor = b;
}

/***************************************************************************************
** Function name:           setTextSize
** Description:             Set the text size multiplier
***************************************************************************************/
void setTextSize(uint8_t s)
{
  if (s>7) s = 7; // Limit the maximum size multiplier so byte variables can be used for rendering
  textsize = (s > 0) ? s : 1; // Don't allow font size 0
}

/***************************************************************************************
** Function name:           textWidth
** Description:             Return the width in pixels of a string in a given font
***************************************************************************************/
int16_t textWidth(const char *string, int font)
{
  unsigned int str_width  = 0;
  char uniCode;
  char *widthtable;

  if (font>1 && font<9)
  widthtable = (char *)pgm_read_word( &(fontdata[font].widthtbl ) ) - 32; //subtract the 32 outside the loop
  else return 0;

  while (*string)
  {
    uniCode = *(string++);
#ifdef LOAD_GLCD
    if (font == 1) str_width += 6;
    else
#endif
    str_width += pgm_read_byte( widthtable + uniCode); // Normally we need to subract 32 from uniCode
  }
  return str_width * textsize;
}


/***************************************************************************************
** Function name:           drawChar
** Description:             draw a single character in the Adafruit GLCD font
***************************************************************************************/
void drawCharGL(int16_t x, int16_t y, unsigned char c, uint16_t color, uint16_t bg, uint8_t size)
{
#ifdef LOAD_GLCD
  if ((x >= (int16_t)X_SIZE)            || // Clip right
      (y >= (int16_t)Y_SIZE)           || // Clip bottom
      ((x + 6 * size - 1) < 0) || // Clip left
      ((y + 8 * size - 1) < 0))   // Clip top
    return;
  bool fillbg = (bg != color);

//spi_begin();

#ifdef FAST_GLCD
// This is about 5 times faster for textsize=1 with background (at 210us per character)
// but it is not really worth the extra 168 bytes needed...
  if ((size==1) && fillbg)
  {
    byte column[6];
    byte mask = 0x1;
    setAddrWindow(x, y, x+5, y+8);
    for (int8_t i = 0; i < 5; i++ ) column[i] = pgm_read_byte(font + (c * 5) + i);
    column[5] = 0;

    for (int8_t j = 0; j < 8; j++) {
      for (int8_t k = 0; k < 5; k++ ) {
        if (column[k] & mask) {
          while (!(SPSR & _BV(SPIF)));
          SPDR = color >> 8; asm volatile( "nop\n\t" ::); // Sync to SPIF bit
          while (!(SPSR & _BV(SPIF)));
          SPDR = color;
        }
        else {
          while (!(SPSR & _BV(SPIF)));
          SPDR = bg >> 8; asm volatile( "nop\n\t" ::);
          while (!(SPSR & _BV(SPIF)));
          SPDR = bg;
        }
      }

      mask <<= 1;
      while (!(SPSR & _BV(SPIF)));
      SPDR = bg >> 8; while (!(SPSR & _BV(SPIF)));
      SPDR = bg;
    }
    while (!(SPSR & _BV(SPIF)));
    TFT_CS_H;
  }
  else
#endif // FAST_GLCD

  {
    for (int8_t i = 0; i < 6; i++ ) {
      uint8_t line;
      if (i == 5)
        line = 0x0;
      else
        line = pgm_read_byte(font + (c * 5) + i);


      if (size == 1) // default size
      {
        for (int8_t j = 0; j < 8; j++) {
          if (line & 0x1) drawPixel(x + i, y + j, color);
        #ifndef FAST_GLCD
          else if (fillbg) drawPixel(x + i, y + j, bg); // Comment out this line if using fast code above
        #endif
          line >>= 1;
        }
      }
      else {  // big size
        for (int8_t j = 0; j < 8; j++) {
          if (line & 0x1) fillRect(x + (i * size), y + (j * size), size, size, color);
          else if (fillbg) fillRect(x + i * size, y + j * size, size, size, bg);
          line >>= 1;
        }
      }
    }
  }
//spi_end();

#endif // LOAD_GLCD
}



/***************************************************************************************
** Function name:           drawChar
** Description:             draw a unicode onto the screen
***************************************************************************************/
int drawChar(unsigned int uniCode, int x, int y, int font)
{
  if (font==1)
  {
#ifdef LOAD_GLCD
      drawCharGL(x, y, uniCode, textcolor, textbgcolor, textsize);
      return 6 * textsize;
#else
      return 0;
#endif
  }

  int width  = 0;
  int height = 0;
  unsigned int flash_address = 0; // 16 bit address OK for Arduino if font files <60K
  uniCode -= 32;

#ifdef LOAD_FONT2
  if (font == 2)
  {
      // This is 20us faster than using the fontdata structure (0.413ms per character instead of 0.433ms)
      flash_address = pgm_read_word(&chrtbl_f16[uniCode]);
      width = pgm_read_byte(widtbl_f16 + uniCode);
      height = chr_hgt_f16;
  }
  #ifdef LOAD_RLE
  else
  #endif
#endif

#ifdef LOAD_RLE
  {
      // This is slower than above but is more convenient for the RLE fonts
      flash_address = pgm_read_word( pgm_read_word( &(fontdata[font].chartbl ) ) + uniCode*sizeof(void *) );
      width = pgm_read_byte( pgm_read_word( &(fontdata[font].widthtbl ) ) + uniCode );
      height= pgm_read_byte( &fontdata[font].height );
  }
#endif



  int w = width;
  int pX      = 0;
  int pY      = y;
  byte line = 0;

#ifdef LOAD_FONT2 // chop out 962 bytes of code if we do not need it
  if (font == 2) {
    
    w = w + 6; // Should be + 7 but we need to compensate for width increment
    w = w / 8;
    if (x + width * textsize >= (int16_t)X_SIZE) return width * textsize ;

    if (textcolor == textbgcolor || textsize != 1) {
      for (int i = 0; i < height; i++)
      {
        if (textcolor != textbgcolor) fillRect(x, pY, width * textsize, textsize, textbgcolor);

        for (int k = 0; k < w; k++)
        {
          line = pgm_read_byte(flash_address + w * i + k);
          if (line) {
            if (textsize == 1) {
              pX = x + k * 8;
              if (line & 0x80) drawPixel(pX, pY, textcolor);
              if (line & 0x40) drawPixel(pX + 1, pY, textcolor);
              if (line & 0x20) drawPixel(pX + 2, pY, textcolor);
              if (line & 0x10) drawPixel(pX + 3, pY, textcolor);
              if (line & 0x08) drawPixel(pX + 4, pY, textcolor);
              if (line & 0x04) drawPixel(pX + 5, pY, textcolor);
              if (line & 0x02) drawPixel(pX + 6, pY, textcolor);
              if (line & 0x01) drawPixel(pX + 7, pY, textcolor);
            }
            else {
              pX = x + k * 8 * textsize;
              if (line & 0x80) fillRect(pX, pY, textsize, textsize, textcolor);
              if (line & 0x40) fillRect(pX + textsize, pY, textsize, textsize, textcolor);
              if (line & 0x20) fillRect(pX + 2 * textsize, pY, textsize, textsize, textcolor);
              if (line & 0x10) fillRect(pX + 3 * textsize, pY, textsize, textsize, textcolor);
              if (line & 0x08) fillRect(pX + 4 * textsize, pY, textsize, textsize, textcolor);
              if (line & 0x04) fillRect(pX + 5 * textsize, pY, textsize, textsize, textcolor);
              if (line & 0x02) fillRect(pX + 6 * textsize, pY, textsize, textsize, textcolor);
              if (line & 0x01) fillRect(pX + 7 * textsize, pY, textsize, textsize, textcolor);
            }
          }
        }
        pY += textsize;
      }
    }
    else
      // Faster drawing of characters and background using block write
    {
      ILI9341_Set_Address(x, y, (x + w * 8) - 1, y + height - 1);

      byte mask;
      for (int i = 0; i < height; i++)
      {
        for (int k = 0; k < w; k++)
        {
          line = pgm_read_byte(flash_address + w * i + k);
          pX = x + k * 8;
          mask = 0x80;
          while (mask) {
            if (line & mask) {
              spi_send16(textcolor);
              /*SPDR = th; asm volatile( "nop\n\t" ::);
              while (!(SPSR & _BV(SPIF)));
              SPDR = tl;
              while (!(SPSR & _BV(SPIF)));*/
              
            }
            else {
              spi_send16(textbgcolor);
              /*SPDR = bh; asm volatile( "nop\n\t" ::);
              while (!(SPSR & _BV(SPIF)));
              SPDR = bl;
              while (!(SPSR & _BV(SPIF)));*/
              
            }
            mask = mask >> 1;
          }
        }
        pY += textsize;
      }
      while (!(SPSR & _BV(SPIF)));
      CS_HIGH();
      //spi_end();
    }
  }

#ifdef LOAD_RLE
  else
  #endif
#endif  //FONT2


  return width * textsize;    // x +
}

/***************************************************************************************
** Function name:           drawString
** Description :            draw string with padding if it is defined
***************************************************************************************/
int drawString(const char *string, int poX, int poY, int font)
{
  int16_t sumX = 0;
  uint8_t padding = 1;
  unsigned int cheight = 0;

  if (textdatum || padX)
  {
    // Find the pixel width of the string in the font
    unsigned int cwidth  = textWidth(string, font);

    // Get the pixel height of the font
    cheight = pgm_read_byte( &fontdata[font].height ) * textsize;

    switch(textdatum) {
      case TC_DATUM:
        poX -= cwidth/2;
        padding = 2;
        break;
      case TR_DATUM:
        poX -= cwidth;
        padding = 3;
        break;
      case ML_DATUM:
        poY -= cheight/2;
        padding = 1;
        break;
      case MC_DATUM:
        poX -= cwidth/2;
        poY -= cheight/2;
        padding = 2;
        break;
      case MR_DATUM:
        poX -= cwidth;
        poY -= cheight/2;
        padding = 3;
        break;
      case BL_DATUM:
        poY -= cheight;
        padding = 1;
        break;
      case BC_DATUM:
        poX -= cwidth/2;
        poY -= cheight;
        padding = 2;
        break;
      case BR_DATUM:
        poX -= cwidth;
        poY -= cheight;
        padding = 3;
        break;
    }
    // Check coordinates are OK, adjust if not
    if (poX < 0) poX = 0;
    if (poX+cwidth>X_SIZE)   poX = X_SIZE - cwidth;
    if (poY < 0) poY = 0;
    if (poY+cheight>Y_SIZE) poY = Y_SIZE - cheight;
  }

  while (*string) sumX += drawChar(*(string++), poX+sumX, poY, font);

//#define PADDING_DEBUG

#ifndef PADDING_DEBUG
  if((padX>sumX) && (textcolor!=textbgcolor))
  {
    int padXc = poX+sumX; // Maximum left side padding
    switch(padding) {
      case 1:
        fillRect(padXc,poY,padX-sumX,cheight, textbgcolor);
        break;
      case 2:
        fillRect(padXc,poY,(padX-sumX)>>1,cheight, textbgcolor);
        padXc = (padX-sumX)>>1;
        if (padXc>poX) padXc = poX;
        fillRect(poX - padXc,poY,(padX-sumX)>>1,cheight, textbgcolor);
        break;
      case 3:
        if (padXc>padX) padXc = padX;
        fillRect(poX + sumX - padXc,poY,padXc-sumX,cheight, textbgcolor);
        break;
    }
  }
#else

  // This is debug code to show text (green box) and blanked (white box) areas
  // to show that the padding areas are being correctly sized and positioned
  if((padX>sumX) && (textcolor!=textbgcolor))
  {
    int padXc = poX+sumX; // Maximum left side padding
    drawRect(poX,poY,sumX,cheight, TFT_GREEN);
    switch(padding) {
      case 1:
        drawRect(padXc,poY,padX-sumX,cheight, TFT_WHITE);
        break;
      case 2:
        drawRect(padXc,poY,(padX-sumX)>>1, cheight, TFT_WHITE);
        padXc = (padX-sumX)>>1;
        if (padXc>poX) padXc = poX;
        drawRect(poX - padXc,poY,(padX-sumX)>>1,cheight, TFT_WHITE);
        break;
      case 3:
        if (padXc>padX) padXc = padX;
        drawRect(poX + sumX - padXc,poY,padXc-sumX,cheight, TFT_WHITE);
        break;
    }
  }
#endif

return sumX;
}

/***************************************************************************************
** Function name:           setTextFont
** Description:             Set the font for the print stream
***************************************************************************************/
void setTextFont(uint8_t f)
{
  textfont = (f > 0) ? f : 1; // Don't allow font 0
}

/***************************************************************************************
** Function name:           write
** Description:             draw characters piped through serial stream
***************************************************************************************/
size_t ILI9341_write(uint8_t uniCode)
{
  if (uniCode == '\r') return 1;
  unsigned int width = 0;
  unsigned int height = 0;
  //Serial.print((char) uniCode); // Debug line sends all printed TFT text to serial port


#ifdef LOAD_FONT2
  if (textfont == 2)
  {
      // This is 20us faster than using the fontdata structure (0.443ms per character instead of 0.465ms)
      width = pgm_read_byte(widtbl_f16 + uniCode-32);
      height = chr_hgt_f16;
      // Font 2 is rendered in whole byte widths so we must allow for this
      width = (width + 6) / 8;  // Width in whole bytes for font 2, should be + 7 but must allow for font width change
      width = width * 8;        // Width converted back to pixles
  }
  #ifdef LOAD_RLE
  else
  #endif
#endif


  #ifdef LOAD_RLE
  {
      // Uses the fontinfo struct array to avoid lots of 'if' or 'switch' statements
      // A tad slower than above but this is not significant and is more convenient for the RLE fonts
      // Yes, this code can be needlessly executed when textfont == 1...
      width = pgm_read_byte( pgm_read_word( &(fontdata[textfont].widthtbl ) ) + uniCode-32 );
      height= pgm_read_byte( &fontdata[textfont].height );
  }
#endif

#ifdef LOAD_GLCD
  if (textfont==1)
  {
      width =  6;
      height = 8;
  }
#else
  if (textfont==1) return 0;
#endif

height = height * textsize;

  if (uniCode == '\n') {
    cursor_y += height;
    cursor_x  = 0;
  }
  else
  {
    if (textwrap && (cursor_x + width * textsize >= X_SIZE))
    {
      cursor_y += height;
      cursor_x = 0;
    }

    cursor_x += drawChar(uniCode, cursor_x, cursor_y, textfont);
  }
  return 1;
}

// stampa semplice stringa
void ILI9341_Print(const char *str) {
    while(*str) {
        ILI9341_write((uint8_t)*str++);
    }
}

uint8_t u16_to_decstr(uint16_t v, char *buf)
{
    uint8_t i = 0;

    if (v == 0) {
        buf[i++] = '0';
    } else {
        char tmp[5];        // max 65535 → 5 cifre
        uint8_t j = 0;

        while (v > 0) {
            tmp[j++] = '0' + (v % 10);
            v /= 10;
        }
        while (j > 0) {
            buf[i++] = tmp[--j];
        }
    }

    buf[i] = '\0';
    return i;               // lunghezza
}

uint8_t u32_to_decstr(uint32_t v, char *buf)
{
    uint8_t i = 0;

    if (v == 0) {
        buf[i++] = '0';
    } else {
        char tmp[10];
        uint8_t j = 0;

        while (v > 0) {
            tmp[j++] = '0' + (v % 10);
            v /= 10;
        }
        while (j > 0) {
            buf[i++] = tmp[--j];
        }
    }

    buf[i] = '\0';
    return i;
}

/*void ILI9341_Draw_String(unsigned int x, unsigned int y, unsigned int color, unsigned int phone, char *str, unsigned char size)
{
	switch (size)
	{
	case 1:
		while (*str)
		{
			if ((x+(size*8))>X_SIZE)
			{
				x = 1;
				y = y + (size*8);
			}
			ILI9341_Draw_Char(x, y, color, phone, *str, size);
			x += size*8;
			*str++;
		}
	break;
	case 2:
		hh=2;
		while (*str)
		{
			if ((x+(size*8))>X_SIZE)
			{
				x = 1;
				y = y + (size*8);
			}
			ILI9341_Draw_Char(x,y,color,phone,*str,size);
			x += hh*8;
			*str++;
		}
	break;
	}
}*/

uint16_t ILI9341_RGB565(uint8_t r, uint8_t g, uint8_t b)
{
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | ((b & 0xF8) >> 3);
}

void ILI9341_draw_rle(const  rle16_t *rle, uint16_t x0, uint16_t y0, uint16_t width)
{
    uint16_t x = x0;
    uint16_t y = y0;
    const struct {uint8_t count; uint16_t color;} *p = rle;

    ILI9341_Set_Address(x0, y0, x0 + width - 1, y0 + 240); // altezza massima display

    while(1)
    {
        uint8_t run = pgm_read_byte(&p->count);
        if(run == 0) break; // terminatore
        uint16_t color = pgm_read_word(&p->color);

        for(uint8_t i = 0; i < run; i++)
        {
            spi_send16(color);
            x++;
            if(x >= x0 + width)
            {
                x = x0;
                y++;
            }
        }
        p++;
    }
}



void draw_rle_bw(uint16_t x0, uint16_t y0, uint16_t width, uint16_t height, const rle_bw_t *rle)
{
    uint16_t x = 0;
    uint16_t y = 0;

    while (1) {
        uint8_t run = pgm_read_byte(&rle->count);
        uint8_t bw  = pgm_read_byte(&rle->color);
        rle++;

        if (run == 0)
            break;

        uint16_t color = bw ? ILI9341_WHITE : ILI9341_BLACK;

        while (run) {
            uint16_t space = width - x;
            uint16_t chunk = (run < space) ? run : space;

            uint16_t x1 = x0 + x;
            uint16_t y1 = y0 + y;
            uint16_t x2 = x1 + chunk - 1;
            uint16_t y2 = y1;

            ILI9341_Set_Address(x1, y1, x2, y2);
            ILI9341_Send_Burst(color, chunk);

            run -= chunk;
            x += chunk;

            if (x >= width) {
                x = 0;
                y++;
                if (y >= height)
                    return;
            }
        }
    }
}


