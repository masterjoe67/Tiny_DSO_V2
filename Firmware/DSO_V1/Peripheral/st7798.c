 #include <avr/io.h>
#include <util/delay.h>
#include <stdbool.h>
#include <avr/pgmspace.h>
#include "st7798.h"
#include "Font16.h"

uint16_t _width = 320;
uint16_t _height = 480;
uint8_t  _rotation = 0;

static uint16_t text_color = 0xFFFF;   // default bianco
static uint16_t text_bg = 0x0000;      // default nero
static uint8_t text_size = 1;

bool  textwrap = true;

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

 // 1. Funzione di attesa: deve essere veloce
// Il bit 7 del registro STATUS nell'FPGA ora ti dice 
// "Aspetta, la FIFO è quasi piena o la SPI sta trasmettendo"
static inline void tft_wait() {
    while (TFT_STATUS & 0x80); 
}

// 2. Invia un comando
void tft_cmd(uint8_t cmd) {
    tft_wait(); 
    TFT_CTRL = (1 << TFT_RST_BIT) | (0 << TFT_DC_BIT); // Imposta DC=0 (Comando)
    TFT_DATA = cmd; // L'FPGA "fotografa" DC=0 e lo mette in FIFO col comando
}

// 3. Invia un dato
void tft_data(uint8_t data) {
    tft_wait();
    TFT_CTRL = (1 << TFT_RST_BIT) | (1 << TFT_DC_BIT); // Imposta DC=1 (Dato)
    TFT_DATA = data; // L'FPGA "fotografa" DC=1 e lo mette in FIFO col dato
}

void tft_init_full_sequence() {
    // --- RESET HARDWARE ---
    TFT_CTRL = 0x00; _delay_ms(120);
    TFT_CTRL = 0x02; _delay_ms(120);

    // --- SEQUENZA RICHIESTA ---
    tft_cmd(0x01); _delay_ms(120); // Software Reset
    tft_cmd(0x11); _delay_ms(120); // Sleep Exit

    // Command Set Control (Unlock)
    tft_cmd(0xF0); tft_data(0xC3);
    tft_cmd(0xF0); tft_data(0x96);

    // Memory Data Access
    tft_cmd(0x36); tft_data(0x48);

    // Interface Pixel Format (16-bit)
    tft_cmd(0x3A); tft_data(0x55);

    // Inversion Control
    tft_cmd(0xB4); tft_data(0x01);

    // Display Function Control
    tft_cmd(0xB6); 
    tft_data(0x80); 
    tft_data(0x02); 
    tft_data(0x3B);

    // Display Output Ctrl Adjust (MOLTO IMPORTANTE)
    tft_cmd(0xE8);
    tft_data(0x40); tft_data(0x8A); tft_data(0x00); tft_data(0x00);
    tft_data(0x29); tft_data(0x19); tft_data(0xA5); tft_data(0x33);

    // Power Control 2 & 3
    tft_cmd(0xC1); tft_data(0x06);
    tft_cmd(0xC2); tft_data(0xA7);

    // VCOM Control
    tft_cmd(0xC5); tft_data(0x18);
    _delay_ms(120);

    // Gamma Sequence (+)
    tft_cmd(0xE0);
    tft_data(0xF0); tft_data(0x09); tft_data(0x0B); tft_data(0x06);
    tft_data(0x04); tft_data(0x15); tft_data(0x2F); tft_data(0x54);
    tft_data(0x42); tft_data(0x3C); tft_data(0x17); tft_data(0x14);
    tft_data(0x18); tft_data(0x1B);

    // Gamma Sequence (-)
    tft_cmd(0xE1);
    tft_data(0xE0); tft_data(0x09); tft_data(0x0B); tft_data(0x06);
    tft_data(0x04); tft_data(0x03); tft_data(0x2B); tft_data(0x43);
    tft_data(0x42); tft_data(0x3B); tft_data(0x16); tft_data(0x14);
    tft_data(0x17); tft_data(0x1B);
    _delay_ms(120);

    // Command Set Control (Lock)
    tft_cmd(0xF0); tft_data(0x3C);
    tft_cmd(0xF0); tft_data(0x69);

    // Display ON
    _delay_ms(120);
    tft_cmd(0x29); 
    _delay_ms(120);
}

void tft_setAddressWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    // Column Address Set (0x2A)
    tft_cmd(0x2A);
    tft_data(x0 >> 8); tft_data(x0 & 0xFF); // Start col
    tft_data(x1 >> 8); tft_data(x1 & 0xFF); // End col

    // Row Address Set (0x2B)
    tft_cmd(0x2B);
    tft_data(y0 >> 8); tft_data(y0 & 0xFF); // Start row
    tft_data(y1 >> 8); tft_data(y1 & 0xFF); // End row

    // Write to RAM (0x2C)
    tft_cmd(0x2C);
}

void tft_fillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    // Imposta la finestra
    tft_setAddressWindow(x, y, x + w - 1, y + h - 1);

    uint8_t hi = color >> 8;
    uint8_t lo = color & 0xFF;

    // Invia i dati del colore per tutti i pixel dell'area
    for (uint32_t i = 0; i < (uint32_t)w * h; i++) {
        tft_data(hi);
        tft_data(lo);
    }
    
    // Comando NOP finale per sicurezza (il nostro trucco per il latch)
    tft_cmd(0x00);
}

void tft_set_cursor(int x, int y) {
    cursor_x = x;
    cursor_y = y;
}

/***************************************************************************************
** Function name:           tft_drawPixel
** Description:             Draw a single pixel at the specified (x,y) coordinate
***************************************************************************************/
void tft_drawPixel(uint16_t x, uint16_t y, uint16_t color) {
    // 1. Controllo dei confini per evitare di scrivere fuori dalla GRAM
    if ((x < 0) || (x >= _width) || (y < 0) || (y >= _height)) return;

    // 2. Imposta la finestra d'indirizzo su un singolo pixel (x,y)
    tft_setAddressWindow(x, y, x, y);

    // 3. Invia i due byte del colore (RGB565)
    tft_data(color >> 8);
    tft_data(color & 0xFF);
    
    // 4. Fine operazione
    tft_cmd(0x00);
}

/***************************************************************************************
** Function name:           tft_drawFastHLine
** Description:             Draw a horizontal line quickly using a single address window
***************************************************************************************/
void tft_drawFastHLine(uint16_t x, uint16_t y, uint16_t w, uint16_t color) {
    tft_setAddressWindow(x, y, x + w - 1, y);
    uint8_t hi = color >> 8;
    uint8_t lo = color & 0xFF;
    while (w--) {
        tft_data(hi);
        tft_data(lo);
    }
}

/***************************************************************************************
** Function name:           tft_drawFastVLine
** Description:             Draw a vertical line quickly using a single address window
***************************************************************************************/
void tft_drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) {
    // 1. Controllo dei confini (Safety first!)
    if ((x < 0) || (x >= _width) || (y >= _height)) return;
    if ((y + h - 1) >= _height) h = _height - y;
    if (h <= 0) return;

    // 2. Apri la finestra verticale: larga 1 pixel, alta H pixel
    tft_setAddressWindow(x, y, x, y + h - 1);

    // 3. Invia i dati colore
    uint8_t hi = color >> 8;
    uint8_t lo = color & 0xFF;
    
    while (h--) {
        tft_data(hi);
        tft_data(lo);
    }
    
    // 4. Chiusura transazione per il latch a 30MHz
    tft_cmd(0x00);
}

/***************************************************************************************
** Function name:           tft_drawRect
** Description:             Draw a rectangle outline with a specific color
***************************************************************************************/
void tft_fillCircle(uint16_t x0, uint16_t y0, uint16_t r, uint16_t color) {
    // Disegna la linea centrale
    tft_drawFastHLine(x0 - r, y0, 2 * r + 1, color);

    int16_t f = 1 - r;
    int16_t ddF_x = 1;
    int16_t ddF_y = -2 * r;
    int16_t x = 0;
    int16_t y = r;

    while (x < y) {
        if (f >= 0) {
            y--;
            ddF_y += 2;
            f += ddF_y;
        }
        x++;
        ddF_x += 2;
        f += ddF_x;

        // Disegna le linee orizzontali tra i punti speculari
        tft_drawFastHLine(x0 - x, y0 + y, 2 * x + 1, color);
        tft_drawFastHLine(x0 - x, y0 - y, 2 * x + 1, color);
        tft_drawFastHLine(x0 - y, y0 + x, 2 * y + 1, color);
        tft_drawFastHLine(x0 - y, y0 - x, 2 * y + 1, color);
    }
    
    // Chiudi la transazione (fondamentale per la tua pull-up su CS!)
    tft_cmd(0x00);
}

void tft_init(){
    tft_init_full_sequence();
}

/***************************************************************************************
** Function name:           tft_setRotation
** Description:             Set display orientation (PORTRAIT, LANDSCAPE, etc.)
***************************************************************************************/
void tft_setRotation(uint8_t m) {
    uint8_t madctl = 0;
    _rotation = m % 4; // Limita a 0-3

    switch (_rotation) {
        case 0: // Verticale Standard
            madctl = 0x40 | 0x80 | 0x08; // MX | MY | BGR
            _width  = 320;
            _height = 480;
            break;
        case 1: // Orizzontale (Landscape) - Ottimo per il DSO
            //madctl = 0x80 | 0x20 | 0x08; // MY | MV | BGR
            madctl = MADCTL_MV | MADCTL_BGR;
            _width  = 480;
            _height = 320;
            break;
        case 2: // Verticale Invertito (Sottosopra)
            madctl = 0x08;               // Solo BGR
            _width  = 320;
            _height = 480;
            break;
        case 3: // Orizzontale Invertito
            madctl = 0x40 | 0x20 | 0x08; // MX | MV | BGR
            _width  = 480;
            _height = 320;
            break;
    }

    tft_cmd(0x36);      // Comando MADCTL
    tft_data(madctl);
    
    // Piccolo trucco per il latch e reset area
    tft_cmd(0x00); 
}

/***************************************************************************************
** Function name:           fillScreen
** Description:             Clear the screen to defined colour
***************************************************************************************/
void tft_fillScreen(uint16_t color)
{
  tft_fillRect(0, 0, _width, _height, color);
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
** Function name:           drawRoundRect
** Description:             Draw a rounded corner rectangle outline
***************************************************************************************/
void tft_drawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color)
{
  // smarter version
  tft_drawFastHLine(x + r  , y    , w - r - r, color); // Top
  tft_drawFastHLine(x + r  , y + h - 1, w - r - r, color); // Bottom
  tft_drawFastVLine(x    , y + r  , h - r - r, color); // Left
  tft_drawFastVLine(x + w - 1, y + r  , h - r - r, color); // Right
  // draw four corners
  tft_drawCircleHelper(x + r    , y + r    , r, 1, color);
  tft_drawCircleHelper(x + w - r - 1, y + r    , r, 2, color);
  tft_drawCircleHelper(x + w - r - 1, y + h - r - 1, r, 4, color);
  tft_drawCircleHelper(x + r    , y + h - r - 1, r, 8, color);
}

/***************************************************************************************
** Function name:           tft_drawCircleHelper
** Description:             Support function for circle drawing
***************************************************************************************/
void tft_drawCircleHelper( int16_t x0, int16_t y0, int16_t r, uint8_t cornername, uint16_t color)
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
      tft_drawPixel(x0 + x, y0 + r, color);
      tft_drawPixel(x0 + r, y0 + x, color);
    }
    if (cornername & 0x2) {
      tft_drawPixel(x0 + x, y0 - r, color);
      tft_drawPixel(x0 + r, y0 - x, color);
    }
    if (cornername & 0x8) {
      tft_drawPixel(x0 - r, y0 + x, color);
      tft_drawPixel(x0 - x, y0 + r, color);
    }
    if (cornername & 0x1) {
      tft_drawPixel(x0 - r, y0 - x, color);
      tft_drawPixel(x0 - x, y0 - r, color);
    }
  }
}

/***************************************************************************************
** Function name:           tft_FillTriangle
** Description:             Draw a filled triangle using scanline interpolation (float version)
***************************************************************************************/
void tft_FillTriangle(Point_t p0, Point_t p1, Point_t p2, uint16_t color)
{
    // 1. Ordinamento dei vertici (Sorting) per coordinata Y: p0 è il più alto, p2 il più basso
    if (p0.y > p1.y) { swap_int16(&p0.y, &p1.y); swap_int16(&p0.x, &p1.x); }
    if (p1.y > p2.y) { swap_int16(&p1.y, &p2.y); swap_int16(&p1.x, &p2.x); }
    if (p0.y > p1.y) { swap_int16(&p0.y, &p1.y); swap_int16(&p0.x, &p1.x); }

    int16_t total_height = p2.y - p0.y;
    if (total_height == 0) return; // Protezione: triangolo degenere (altezza zero)

    // 2. Loop di scansione riga per riga
    for (int16_t y = p0.y; y <= p2.y; y++) {
        // Determina se siamo nella metà superiore o inferiore del triangolo
        uint8_t second_half = y > p1.y || p1.y == p0.y;
        int16_t segment_height = second_half ? p2.y - p1.y : p1.y - p0.y;
        if (segment_height == 0) continue; 

        // 3. Calcolo dei coefficienti di interpolazione (0.0 a 1.0)
        float alpha = (float)(y - p0.y) / total_height;
        float beta  = (float)(y - (second_half ? p1.y : p0.y)) / segment_height;

        // 4. Calcolo delle coordinate X iniziali e finali per la riga corrente
        int16_t ax = p0.x + (p2.x - p0.x) * alpha;
        int16_t bx = second_half
            ? p1.x + (p2.x - p1.x) * beta
            : p0.x + (p1.x - p0.x) * beta;

        // 5. Riempimento: disegna una linea orizzontale tra i due lati del triangolo
        if (ax > bx) swap_int16(&ax, &bx);
        int16_t w = bx - ax;
        tft_drawFastHLine(ax, y, w, color);
    }
}

/***************************************************************************************
** Function name:           tft_drawChar
** Description:             Draw a character at (x,y) using internal flash fonts
***************************************************************************************/
int tft_drawChar(unsigned int uniCode, int x, int y, int font) {
    if (font == 1) {
#ifdef LOAD_GLCD
        tft_drawCharGL(x, y, uniCode, textcolor, textbgcolor, textsize);
        return 6 * textsize;
#else
        return 0;
#endif
    }

    int width = 0;
    int height = 0;
    uint32_t flash_address = 0; 
    uniCode -= 32;

#ifdef LOAD_FONT2
    if (font == 2) {
        flash_address = pgm_read_word(&chrtbl_f16[uniCode]);
        width = pgm_read_byte(widtbl_f16 + uniCode);
        height = chr_hgt_f16;
    }
#endif

    int w = width;
    int pY = y;
    uint8_t line = 0;

#ifdef LOAD_FONT2
    if (font == 2) {
        w = (w + 7) / 8; // Calcola quanti byte occupa la riga nel font
        
        // Controllo fuori schermo (X)
        if (x + width * textsize >= (int16_t)_width) return width * textsize;

        // Caso 1: Font Scalato o Trasparente (Lento, pixel per pixel)
        if (textcolor == textbgcolor || textsize != 1) {
            for (int i = 0; i < height; i++) {
                if (textcolor != textbgcolor) tft_fillRect(x, pY, width * textsize, textsize, textbgcolor);

                for (int k = 0; k < w; k++) {
                    line = pgm_read_byte(flash_address + w * i + k);
                    if (line) {
                        int pX = x + k * 8 * textsize;
                        for (int bit = 0; bit < 8; bit++) {
                            if (line & (0x80 >> bit)) {
                                if (textsize == 1) tft_drawPixel(pX + bit, pY, textcolor);
                                else tft_fillRect(pX + bit * textsize, pY, textsize, textsize, textcolor);
                            }
                        }
                    }
                }
                pY += textsize;
            }
        }
        // Caso 2: Font Standard (Veloce, scrittura a blocchi)
        else {
            // Usa la tua funzione per aprire la finestra di scrittura
            tft_setAddressWindow(x, y, (x + w * 8) - 1, y + height - 1);

            for (int i = 0; i < height; i++) {
                for (int k = 0; k < w; k++) {
                    line = pgm_read_byte(flash_address + w * i + k);
                    for (int bit = 0; bit < 8; bit++) {
                        // Spara il colore direttamente nel bus a 30MHz
                        if (line & (0x80 >> bit)) tft_data16(textcolor);
                        else tft_data16(textbgcolor);
                    }
                }
            }
            tft_cmd(0x00); // Chiude la transazione
        }
    }
#endif

    return width * textsize;
}

/***************************************************************************************
** Function name:           write
** Description:             draw characters piped through serial stream
***************************************************************************************/
size_t tft_write(uint8_t uniCode)
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
    if (textwrap && (cursor_x + width * textsize >= _width))
    {
      cursor_y += height;
      cursor_x = 0;
    }

    cursor_x += tft_drawChar(uniCode, cursor_x, cursor_y, textfont);
  }
  return 1;
}

/***************************************************************************************
** Function name:           ILI9341_Print
** Description:             Send a string to the display char by char
***************************************************************************************/
void tft_Print(const char *str) {
    while(*str) {
        // Invia il carattere al generatore di font/caratteri
        tft_write((uint8_t)*str++);
    }
}

/***************************************************************************************
** Function name:           u16_to_decstr
** Description:             Convert a 16-bit unsigned integer to a decimal string
***************************************************************************************/
uint8_t u16_to_decstr(uint16_t v, char *buf)
{
    uint8_t i = 0;

    if (v == 0) {
        buf[i++] = '0';
    } else {
        char tmp[5];        // Buffer temporaneo: max 65535 → 5 cifre
        uint8_t j = 0;

        // Estrae le cifre una per una partendo dall'ultima (modulo 10)
        while (v > 0) {
            tmp[j++] = '0' + (v % 10);
            v /= 10;
        }
        
        // Inverte l'ordine delle cifre per memorizzarle correttamente nel buffer
        while (j > 0) {
            buf[i++] = tmp[--j];
        }
    }

    buf[i] = '\0';          // Termina la stringa
    return i;               // Restituisce la lunghezza per posizionamento rapido
}

/***************************************************************************************
** Function name:           tft_drawLine
** Description:             Draw a line between (x1,y1) and (x2,y2) using Bresenham
***************************************************************************************/
void tft_drawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color) {
    // 1. Caso speciale: Linee perfettamente dritte
    // Funzionano perfettamente, quindi le usiamo come "ancora di salvezza"
    if (x1 == x2) {
        tft_drawFastVLine(x1, (y1 < y2 ? y1 : y2), abs((int16_t)y2 - (int16_t)y1) + 1, color);
        return;
    }
    if (y1 == y2) {
        tft_drawFastHLine((x1 < x2 ? x1 : x2), y1, abs((int16_t)x2 - (int16_t)x1) + 1, color);
        return;
    }

    // 2. Trasformiamo in int16_t per gestire correttamente i segni nei calcoli
    int16_t x = x1;
    int16_t y = y1;
    int16_t x_fine = x2;
    int16_t y_fine = y2;

    bool steep = abs(y_fine - y) > abs(x_fine - x);
    if (steep) {
        // Swap x con y
        int16_t tmp;
        tmp = x; x = y; y = tmp;
        tmp = x_fine; x_fine = y_fine; y_fine = tmp;
    }

    if (x > x_fine) {
        // Swap inizio con fine
        int16_t tmp;
        tmp = x; x = x_fine; x_fine = tmp;
        tmp = y; y = y_fine; y_fine = tmp;
    }

    int16_t dx = x_fine - x;
    int16_t dy = abs(y_fine - y);
    int16_t err = dx / 2;
    int16_t ystep = (y < y_fine) ? 1 : -1;

    // 3. Ciclo di disegno
    for (; x <= x_fine; x++) {
        // Se steep è attivo, dobbiamo "dis-invertire" le coordinate per il pixel
        if (steep) tft_drawPixel(y, x, color);
        else       tft_drawPixel(x, y, color);

        err -= dy;
        if (err < 0) {
            y += ystep;
            err += dx;
        }
    }
}

/***************************************************************************************
** Function name:           tft_data16
** Description:             Invia 16 bit di colore (RGB565) al display
***************************************************************************************/
static inline void tft_data16(uint16_t color) {
    // Invia il byte alto (Most Significant Byte)
    // Spostiamo i bit a destra di 8 per isolare i 5 bit del Rosso e i primi 3 del Verde
    tft_data((uint8_t)(color >> 8));
    
    // Invia il byte basso (Least Significant Byte)
    // Mascheriamo per sicurezza, prendendo i restanti 3 bit del Verde e i 5 del Blu
    tft_data((uint8_t)(color & 0xFF));
}

/***************************************************************************************
** Function name:           tft_drawCharGL
** Description:             Draw a single character in the Adafruit GLCD font (5x7)
***************************************************************************************/
void tft_drawCharGL(int16_t x, int16_t y, unsigned char c, uint16_t color, uint16_t bg, uint8_t size)
{
#ifdef LOAD_GLCD
    // 1. Clipping: evita di scrivere fuori dai margini dello schermo
    if ((x >= (int16_t)_width) || (y >= (int16_t)_height) || 
        ((x + 6 * size - 1) < 0) || ((y + 8 * size - 1) < 0)) return;

    bool fillbg = (bg != color);

    // 2. Ottimizzazione per size 1 con sfondo (molto veloce)
    if ((size == 1) && fillbg)
    {
        uint8_t column[6];
        uint8_t mask = 0x01;
        
        // Imposta la finestra 6x8 (5 pixel font + 1 spazio)
        tft_setAddressWindow(x, y, x + 5, y + 7);

        for (int8_t i = 0; i < 5; i++) column[i] = pgm_read_byte(font + (c * 5) + i);
        column[5] = 0; // Spazio tra i caratteri

        // L'ST7796S scrive per righe, ma il font GLCD è memorizzato per colonne
        for (int8_t j = 0; j < 8; j++) {
            for (int8_t k = 0; k < 6; k++) {
                if (column[k] & mask) {
                    tft_data16(color);
                } else {
                    tft_data16(bg);
                }
            }
            mask <<= 1;
        }
        tft_cmd(0x00); // Chiude la transazione
    }
    // 3. Caso per caratteri scalati o senza sfondo
    else
    {
        for (int8_t i = 0; i < 6; i++) {
            uint8_t line;
            if (i == 5) line = 0x0;
            else        line = pgm_read_byte(font + (c * 5) + i);

            for (int8_t j = 0; j < 8; j++) {
                if (line & 0x1) {
                    if (size == 1) tft_drawPixel(x + i, y + j, color);
                    else           tft_fillRect(x + (i * size), y + (j * size), size, size, color);
                } 
                else if (fillbg) {
                    if (size == 1) tft_drawPixel(x + i, y + j, bg);
                    else           tft_fillRect(x + (i * size), y + (j * size), size, size, bg);
                }
                line >>= 1;
            }
        }
    }
#endif
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
** Function name:           tft_drawRect
** Description:             Draw a rectangle outline using fast lines
***************************************************************************************/
void tft_drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    // Disegna i quattro lati del rettangolo
    tft_drawFastHLine(x, y, w, color);           // Lato superiore
    tft_drawFastHLine(x, y + h - 1, w, color);   // Lato inferiore
    tft_drawFastVLine(x, y, h, color);           // Lato sinistro
    tft_drawFastVLine(x + w - 1, y, h, color);   // Lato destro
}

/***************************************************************************************
** Function name:           tft_printAt
** Description:             Stampa una stringa usando la tua tft_drawChar
***************************************************************************************/
void tft_printAt(const char *str, int16_t x, int16_t y, uint16_t color, uint16_t bg) {
    // Nota: Ho rimosso il parametro font dalla chiamata esterna per semplicità,
    // forzando il font 1 (GLCD) o quello che preferisci per la UI.
    int16_t curX = x;
    
    // Impostiamo i colori globali (textcolor e textbgcolor) 
    // prima di chiamare tft_drawChar, dato che lei li usa internamente.
    textcolor = color;
    textbgcolor = bg;
    textsize = 1; // Dimensione standard per i piccoli testi del DSO

    while (*str) {
        // Usiamo la tua funzione!
        // uniCode è il carattere attuale (*str)
        // font = 1 (quello che abbiamo adattato prima)
        curX += tft_drawChar((unsigned int)*str++, curX, y, 1);
        
        // Protezione margine destro
        if (curX > _width - 5) break;
    }
}

int test() {
    // Inizializzazione fisica
    tft_init_full_sequence();

    // Disegno del rettangolo (320x480)
    tft_cmd(0x2C); // RAM Write
    for(uint32_t i = 0; i < 153600UL; i++) {
        tft_data(0xF8); // Rosso MSB
        tft_data(0x50); // Rosso LSB
    }

    // --- IL TRUCCO PER IL LATCH ---
    // Inviamo un comando NOP e alziamo CS (gestito dal VHDL)
    // Questo forza il controller a visualizzare la GRAM
    //tft_cmd(0x00); 
   // _delay_ms(10);

    // Se ancora non vedi nulla, prova a forzare l'inversione
    // tft_cmd(0x21); 

    while(1) {
        // Rimani qui
    }
    return 0;
}