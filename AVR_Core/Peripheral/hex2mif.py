# converte sin_lut_2048.hex -> sin_lut_2048.mif
# Assumiamo valori esadecimali, una riga per valore, fino a 2048 righe

input_file  = "sin_lut_2048.hex"
output_file = "sin_lut_2048.mif"
N = 2048
WIDTH = 10

with open(input_file, "r") as f:
    lines = [line.strip() for line in f.readlines()]

if len(lines) != N:
    print(f"Attenzione: il file ha {len(lines)} righe, previsto {N}")

with open(output_file, "w") as f:
    f.write(f"DEPTH = {N};\n")
    f.write(f"WIDTH = {WIDTH};\n")
    f.write("ADDRESS_RADIX = HEX;\n")
    f.write("DATA_RADIX = HEX;\n")
    f.write("CONTENT BEGIN\n")
    for addr, val in enumerate(lines):
        addr_hex = format(addr, 'X')
        val_hex  = val.upper().replace("0X","")
        f.write(f"  {addr_hex} : {val_hex};\n")
    f.write("END;\n")

print(f"{output_file} generato correttamente per Quartus!")
