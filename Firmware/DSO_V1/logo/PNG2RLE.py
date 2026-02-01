from PIL import Image

def rgb_to_rgb565(r, g, b):
    """Converte RGB 8-bit in RGB565 (uint16)"""
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)

def image_to_rle(img_path, mode='RGB565'):
    """Converte immagine in array RLE"""
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

def save_rle_to_file(rle_data, filename='logo_rle.h', array_name='logo_rle', mode='RGB565'):
    """Salva l’array RLE in un file .h pronto per AVR"""
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

# --- USO ---
img_path = 'logo.png'
rle = image_to_rle(img_path, mode='RGB565')
save_rle_to_file(rle, filename='logo_mje.h', array_name='logo_mje', mode='RGB565')
