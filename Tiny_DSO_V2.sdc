# Crea il clock d'ingresso (es. 50MHz sul pin fisico)
create_clock -name CLOCK_50 -period 20.000 [get_ports CLOCK_50]

# Chiedi a Quartus di calcolare automaticamente i clock che escono dal PLL
derive_pll_clocks

# Calcola le incertezze del clock (jitter, ecc.)
derive_clock_uncertainty