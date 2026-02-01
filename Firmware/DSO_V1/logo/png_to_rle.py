import tkinter as tk
from tkinter import filedialog, messagebox
from PIL import Image

# --- Funzioni di conversione ---
def rgb_to_rgb565(r, g, b):
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)

def image_to_rle(img_path, mode='RGB565'):
    img = Image.open(img_path).convert('RGB')
    w, h = img.size
    rle_data = []

    for y in range(h):
        x = 0
        while x < w:
            r, g, b = img.getpixel((x, y))
            color = rgb_to_rgb565(r, g, b) if mode == 'RGB565' else (1 if (r+g+b)>384 else 0)

            # run-length
            run = 1
            while (x + run < w):
                r1, g1, b1 = img.getpixel((x+run, y))
                color1 = rgb_to_rgb565(r1, g1, b1) if mode == 'RGB565' else (1 if (r1+g1+b1)>384 else 0)
                if color1 != color or run >= 255:
                    break
                run += 1

            rle_data.append((run, color))
            x += run

    return rle_data

def save_rle_to_file(rle_data, filename, array_name='logo_rle', mode='RGB565'):
    with open(filename, 'w') as f:
        f.write(f'#include <avr/pgmspace.h>\n\n')
        if mode == 'RGB565':
            f.write(f'const struct {{uint8_t count; uint16_t color;}} {array_name}[] PROGMEM = {{\n')
        else:
            f.write(f'const struct {{uint8_t count; uint8_t color;}} {array_name}[] PROGMEM = {{\n')
        for run, color in rle_data:
            if mode == 'RGB565':
                f.write(f'    {{{run}, 0x{color:04X}}},\n')
            else:
                f.write(f'    {{{run}, {color}}},\n')
        f.write('    {0, 0} // terminatore\n};\n')
    print(f"[OK] Array salvato in {filename}")

# --- Funzioni GUI ---
def select_input_file():
    path = filedialog.askopenfilename(filetypes=[("PNG Images","*.png")])
    if path:
        entry_input.delete(0, tk.END)
        entry_input.insert(0, path)

def select_output_file():
    path = filedialog.asksaveasfilename(defaultextension=".h", filetypes=[("Header Files","*.h")])
    if path:
        entry_output.delete(0, tk.END)
        entry_output.insert(0, path)

def convert_image():
    input_file = entry_input.get()
    output_file = entry_output.get()
    array_name = entry_array.get().strip() or 'logo_rle'
    mode = mode_var.get()

    if not input_file or not output_file:
        messagebox.showerror("Errore", "Seleziona file di input e output")
        return

    try:
        rle = image_to_rle(input_file, mode=mode)
        save_rle_to_file(rle, output_file, array_name=array_name, mode=mode)
        messagebox.showinfo("Fatto", f"File RLE salvato in:\n{output_file}")
    except Exception as e:
        messagebox.showerror("Errore", str(e))

# --- GUI ---
root = tk.Tk()
root.title("PNG -> RLE Converter")

tk.Label(root, text="File PNG:").grid(row=0, column=0, padx=5, pady=5, sticky='e')
entry_input = tk.Entry(root, width=50)
entry_input.grid(row=0, column=1, padx=5, pady=5)
tk.Button(root, text="Sfoglia...", command=select_input_file).grid(row=0, column=2, padx=5, pady=5)

tk.Label(root, text="File output (.h):").grid(row=1, column=0, padx=5, pady=5, sticky='e')
entry_output = tk.Entry(root, width=50)
entry_output.grid(row=1, column=1, padx=5, pady=5)
tk.Button(root, text="Salva come...", command=select_output_file).grid(row=1, column=2, padx=5, pady=5)

tk.Label(root, text="Nome array:").grid(row=2, column=0, padx=5, pady=5, sticky='e')
entry_array = tk.Entry(root, width=50)
entry_array.insert(0, "logo_rle")
entry_array.grid(row=2, column=1, padx=5, pady=5, columnspan=2)

tk.Label(root, text="Modalità:").grid(row=3, column=0, padx=5, pady=5, sticky='e')
mode_var = tk.StringVar(value="RGB565")
tk.OptionMenu(root, mode_var, "RGB565", "BW").grid(row=3, column=1, padx=5, pady=5, sticky='w')

tk.Button(root, text="Converti", command=convert_image, width=20, bg="lightgreen").grid(row=4, column=0, columnspan=3, pady=10)

root.mainloop()
