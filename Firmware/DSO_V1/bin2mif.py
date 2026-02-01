#!/usr/bin/env python3
import sys

binfile = sys.argv[1]   # es. main.bin
miffile = sys.argv[2]   # es. main.mif
width = int(sys.argv[3])  # larghezza in bit
depth = int(sys.argv[4])  # numero di parole

with open(binfile, "rb") as f:
    data = f.read()

with open(miffile, "w") as f:
    f.write(f"WIDTH={width};\n")
    f.write(f"DEPTH={depth};\n")
    f.write("ADDRESS_RADIX=UNS;\n")
    f.write("DATA_RADIX=HEX;\n")
    f.write("CONTENT BEGIN\n")
    
    for addr in range(depth):
        idx = addr*2
        if idx+1 < len(data):
            word = (data[idx+1] << 8) | data[idx]
        elif idx < len(data):
            word = data[idx]
        else:
            word = 0
        f.write(f"  {addr} : {word:04X};\n")
    
    f.write("END;\n")
