#include <avr/io.h>
#include <util/delay.h>

// Definiamo i registri basandoci sulla tua mappatura
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



static const uint8_t st7796s_init_data[] = {
    0x01, ST_DELAY_BIT | 0, 150,          // SWRESET + 150ms delay
    0xF0, 1, 0xC3,                     // Unlock
    0xF0, 1, 0x96,                     // Unlock
    0xC5, 1, 0x1C,                     // VCOM
    0x36, 1, 0x48,                     // MADCTL
    0x3A, 1, 0x55,                     // COLMOD 16-bit
    0xB0, 1, 0x80,                     // Interface Control
    0xB4, 1, 0x00,                     // Inversion Control
    0xB6, 3, 0x80, 0x22, 0x3B,         // Display Function Control (I 3 famosi argomenti!)
    0xB7, 1, 0xC6,                     // Entry Mode
    0xF0, 1, 0x69,                     // Lock
    0xF0, 1, 0x3C,                     // Lock
    0x11, ST_DELAY_BIT | 0, 150,          // Sleep Out + 150ms delay
    0x29, ST_DELAY_BIT | 0, 150           // Display ON + 150ms delay
};

void tft_init_adafruit_pure_c() {
    int i = 0;
    uint8_t num_commands = 14; 

    while (num_commands--) {
        uint8_t cmd = st7796s_init_data[i++];
        uint8_t arg_raw = st7796s_init_data[i++];
        uint8_t num_args = arg_raw & ~ST_DELAY_BIT;
        
        tft_cmd(cmd);
        
        for (uint8_t j = 0; j < num_args; j++) {
            tft_data(st7796s_init_data[i++]);
        }
        
        if (arg_raw & ST_DELAY_BIT) {
            uint16_t ms = st7796s_init_data[i++];
            if (ms == 255) ms = 500;
            
            // SOLUZIONE ALL'ERRORE:
            // Usiamo un ciclo per generare il ritardo variabile
            while(ms--) {
                _delay_ms(1); 
            }
        }
    }
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

// Definiamo i codici comando per chiarezza



int main() {
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