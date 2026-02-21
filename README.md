# 🚀 Tiny DSO - AVR & FPGA Core

Sistema integrato basato su **AVR Core** implementato in FPGA, ottimizzato per il controllo di un display **ST7796** tramite controller SPI ad alta velocità.

<p align="center">
  <img src="assets/preview.png" alt="Tiny DSO Preview" width="600">
</p>

## 🛠️ Caratteristiche Hardware (VHDL)

Il progetto implementa diversi moduli custom per massimizzare le performance dell'AVR:

* **Super SPI Controller**: Driver ottimizzato per display ST7796 con supporto FIFO e gestione automatica dei segnali DC/CS.
* **Backlight PWM**: Controllo della luminosità del display a 8-bit (~195kHz) tramite registro MMIO dedicato.
* **Smart Encoder Bank**: Gestione di 7 encoder rotativi con debounce hardware filtrato a 4.3ms (18-bit) per eliminare l'effetto "ping-pong".
* **Expanded Keypad**: Matrice 5x3 integrata con 4 tasti encoder diretti (totale 19 tasti) mappati in un unico spazio di memoria.

## 💻 Mappatura Memoria (MMIO)

I tasti sono accessibili via firmware in modo trasparente:
- **Codici 0-14**: Matrice tastiera principale.
- **Codici 15-18**: Tasti integrati negli encoder (ENC_KEYS).

## 🚀 Guida Rapida
1. **Quartus**: Assegnare i pin e attivare i `Weak Pull-Up Resistor` per gli ingressi `enc_keys_i` e `rows_i`.
2. **Firmware**: Utilizzare la routine di lettura atomica per i registri a 16-bit degli encoder per evitare glitch sui byte HI/LO.
3. **Backlight**: Scrivere un valore da `0x00` a `0xFF` nell'indirizzo `LCD_PWM_REG` per regolare la luminosità.

---
*Developed with MJE - 2026*