from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parent.parent
OUT_H = ROOT/'include'/'menu_assets.h'
OUT_CPP = ROOT/'source'/'menu_assets.cpp'
FONT_PATH = ROOT/'assets'/'fonts'/'Minecraft.ttf'
BG_PATH = ROOT/'assets'/'bg.png'
LOGO_PATH = ROOT/'assets'/'logo.png'

TEXTS = [
    ('TITLE_NEW_GAME', 'NEW GAME', 20),
    ('TITLE_LOAD_GAME', 'LOAD GAME', 20),
    ('PAUSE_RESUME', 'RESUME', 20),
    ('PAUSE_SAVE_GAME', 'SAVE GAME', 20),
    ('PAUSE_QUIT_GAME', 'QUIT GAME', 20),
    ('LABEL_LOADING', 'LOADING...', 18),
    ('LABEL_PAUSED', 'PAUSED', 18),
]

try:
    from PIL import Image, ImageFont, ImageDraw
except ModuleNotFoundError:
    if OUT_H.exists() and OUT_CPP.exists():
        print('Pillow not installed; reusing checked-in menu assets:')
        print(f'  {OUT_H}')
        print(f'  {OUT_CPP}')
        sys.exit(0)
    print('Error: Pillow is required to generate menu assets from PNG/TTF,')
    print('and no pre-generated menu_assets files were found to reuse.')
    print('Install it with: python3 -m pip install Pillow')
    sys.exit(1)


def rgb15(r, g, b):
    return ((b >> 3) << 10) | ((g >> 3) << 5) | (r >> 3) | 0x8000


def img_to_u16(im, transparent=False):
    rgba = im.convert('RGBA')
    out = []
    for y in range(rgba.height):
        for x in range(rgba.width):
            r, g, b, a = rgba.getpixel((x, y))
            if transparent and a < 32:
                out.append(0)
            else:
                out.append(rgb15(r, g, b))
    return out


def render_text(text, size):
    font = ImageFont.truetype(str(FONT_PATH), size)
    dummy = Image.new('RGBA', (4, 4), (0, 0, 0, 0))
    draw = ImageDraw.Draw(dummy)
    bbox = draw.textbbox((0, 0), text, font=font)
    pad = 4
    w = bbox[2] - bbox[0] + pad * 2
    h = bbox[3] - bbox[1] + pad * 2
    im = Image.new('RGBA', (w, h), (0, 0, 0, 0))
    draw = ImageDraw.Draw(im)
    for ox, oy in [(-1,0),(1,0),(0,-1),(0,1),(-1,-1),(1,-1),(-1,1),(1,1)]:
        draw.text((pad + ox, pad + oy), text, font=font, fill=(0,0,0,255))
    draw.text((pad, pad), text, font=font, fill=(255,255,255,255))
    return im


def write_array(f, name, data):
    f.write(f'const unsigned short {name}[] = {{\n')
    for i in range(0, len(data), 12):
        row = ', '.join(f'0x{v:04X}' for v in data[i:i+12])
        f.write('    ' + row + ',\n')
    f.write('};\n\n')

bg = Image.open(BG_PATH).convert('RGBA')
logo = Image.open(LOGO_PATH).convert('RGBA')
bg_full = bg.resize((256, 192), Image.NEAREST)
logo_small = logo
texts = []
for name, text, size in TEXTS:
    texts.append((name, render_text(text, size)))

with open(OUT_H, 'w') as h:
    h.write('#pragma once\n#include <nds.h>\n\n')
    h.write('extern const int MENU_BG_W;\nextern const int MENU_BG_H;\nextern const unsigned short MENU_BG[];\n')
    h.write('extern const int MENU_LOGO_W;\nextern const int MENU_LOGO_H;\nextern const unsigned short MENU_LOGO[];\n')
    for name, im in texts:
        h.write(f'extern const int {name}_W;\nextern const int {name}_H;\nextern const unsigned short {name}[];\n')

with open(OUT_CPP, 'w') as cpp:
    cpp.write('#include "menu_assets.h"\n\n')
    cpp.write('const int MENU_BG_W = 256;\nconst int MENU_BG_H = 192;\n')
    write_array(cpp, 'MENU_BG', img_to_u16(bg_full, transparent=False))
    cpp.write(f'const int MENU_LOGO_W = {logo_small.width};\nconst int MENU_LOGO_H = {logo_small.height};\n')
    write_array(cpp, 'MENU_LOGO', img_to_u16(logo_small, transparent=True))
    for name, im in texts:
        cpp.write(f'const int {name}_W = {im.width};\nconst int {name}_H = {im.height};\n')
        write_array(cpp, name, img_to_u16(im, transparent=True))

print(f'Generated {OUT_CPP}')
print(f'Generated {OUT_H}')
