# -------------------------------------------------------------------
# Configurazione
# -------------------------------------------------------------------
ARCH      = atmega128
CC        = avr-gcc
AS        = avr-as
OBJCOPY   = avr-objcopy
OBJDUMP   = avr-objdump
F_CPU     = 40000000UL

# --- ELENCO FILE SORGENTE ---
# aggiungi qui i tuoi file .c
SRCS = main.c \
       Peripheral/st7798.c\
	   Peripheral/uart.c\
	   Peripheral/leds.c\
	   Peripheral/input.c\
	   scope.c
#glcdfont.c
#scope.c
	   


# converte .c → .o
OBJS = $(SRCS:.c=.o)

ELF  = firmware.elf
HEX  = firmware.hex
BIN  = firmware.bin
MIF  = firmware.mif
LST  = firmware.lst

CFLAGS  = -mmcu=$(ARCH) -DF_CPU=$(F_CPU) -funsigned-char -funsigned-bitfields \
          -fpack-struct -fshort-enums -Wall -Wstrict-prototypes \
          -Wno-unused-function -Os
LDFLAGS = -mmcu=$(ARCH)

PMEM_DEPTH=14
SRAM_DEPTH=12
PMEM_WORDS=$(shell powershell -Command "$((1<<$(PMEM_DEPTH)))")
FLASH_SIZE = $(shell powershell -Command "$(( $(PMEM_WORDS) * 2 ))") 
SRAM_SIZE  = $(shell powershell -Command "$((1<<$(SRAM_DEPTH)))")

# -------------------------------------------------------------------
# Build
# -------------------------------------------------------------------
all: $(HEX) $(MIF) $(LST) report

# Compilazione oggetti
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Link finale
$(ELF): $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) -o $(ELF)

$(HEX): $(ELF)
	$(OBJCOPY) -O ihex $(ELF) $(HEX)

$(BIN): $(ELF)
	$(OBJCOPY) -O binary $(ELF) $(BIN)

# -------------------------------------------------------------------
# MIF
# -------------------------------------------------------------------
$(MIF): $(BIN)
	python3 bin2mif.py $(BIN) $(MIF) 16 16384

# -------------------------------------------------------------------
# Listing
# -------------------------------------------------------------------
$(LST): $(ELF)
	$(OBJDUMP) -d $(ELF) > $(LST)

# -------------------------------------------------------------------
# Report memoria
# -------------------------------------------------------------------
report:
	@echo "---------------------------------------"
	@echo "Core sintetizzato: $(ARCH)"
	@echo "Flash: $(FLASH_SIZE) bytes ($(PMEM_WORDS) parole da 2 byte)"
	@echo "SRAM:  $(SRAM_SIZE) bytes"
	@echo "---------------------------------------"
	@avr-size --format=avr $(ELF)

# -------------------------------------------------------------------
# Clean
# -------------------------------------------------------------------
clean:
	rm -f $(ELF) $(HEX) $(BIN) $(MIF) $(LST) $(OBJS)

.PHONY: all clean report
