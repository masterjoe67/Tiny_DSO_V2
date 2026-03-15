#include "trigger_manager.h"
#include "scope_shared.h"
#include "fpga_manager.h"

// Routine per calcolare lo step dinamico del trigger
// ch->volts_div: valore assoluto (0-4096) che rappresenta il range -5V/+5V
uint16_t calcola_step_trigger(float current_vdiv) {
    
    // Gestione scale millivolt (0.01V - 0.05V)
    if (current_vdiv <= 0.021f) return 1;   // 10mV e 20mV: precisione massima
    if (current_vdiv <= 0.051f) return 4;   // 50mV: 1 scatto ~ 2.5 pixel
    
    // Gestione scale medie (100mV - 500mV)
    if (current_vdiv <= 0.11f)  return 8;   // 100mV
    if (current_vdiv <= 0.21f)  return 16;  // 200mV
    if (current_vdiv <= 0.51f)  return 40;  // 500mV
    
    // Gestione scale alte (1V - 10V)
    if (current_vdiv <= 1.1f)   return 80;  // 1V
    if (current_vdiv <= 2.1f)   return 160; // 2V
    if (current_vdiv <= 5.1f)   return 400; // 5V
    
    return 800; // Default per 10V
}

void scope_set_hysteresis(uint8_t value) {
    // Valore tipico: 10-50. 
    // Se il segnale è sporco, alza questo valore.
    XRAM_WRITE(REG_TRIG_HYST, value);
}