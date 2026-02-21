# 🚀 Tiny DSO - AVR & FPGA Core

Sistema integrato basato su **AVR Core** implementato in FPGA, ottimizzato per il controllo di un display **ST7796** tramite controller SPI ad alta velocità.

<p align="center">
  <img src="assets/screenshot.png" alt="Tiny DSO Preview" width="600">
</p>

## 🛠️ Caratteristiche Hardware (VHDL)

Il progetto implementa diversi moduli custom per massimizzare le performance dell'AVR:

* **Super SPI Controller**: Driver ottimizzato per display ST7796 con supporto FIFO e gestione automatica dei segnali DC/CS.
* **Backlight PWM**: Controllo della luminosità del display a 8-bit (~195kHz) tramite registro MMIO dedicato.
* **Smart Encoder Bank**: Gestione di 7 encoder rotativi con debounce hardware filtrato a 4.3ms (18-bit) per eliminare l'effetto "ping-pong".
* **Expanded Keypad**: Matrice 5x3 integrata con 4 tasti encoder diretti (totale 19 tasti) mappati in un unico spazio di memoria.



---
*Developed by MJE - 2026*