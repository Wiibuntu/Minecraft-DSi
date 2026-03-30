#!/usr/bin/env python3
from __future__ import annotations
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / 'textures' / 'mobs' / 'pig.png'
OUT = ROOT / 'source' / 'mob_assets.cpp'
W, H = 64, 32

def rgb5551(r,g,b,a):
    if a < 32:
        return 0
    return ((r >> 3) & 31) | (((g >> 3) & 31) << 5) | (((b >> 3) & 31) << 10) | 0x8000

img = Image.open(SRC).convert('RGBA').resize((W, H), Image.NEAREST)
vals=[]
for y in range(H):
    for x in range(W):
        vals.append(rgb5551(*img.getpixel((x,y))))

lines=['#include "mob_assets.h"','',f'const u16 TEX_MOB_PIG[MOB_TEX_W * MOB_TEX_H] = {{']
for i in range(0,len(vals),8):
    chunk=vals[i:i+8]
    lines.append('    ' + ', '.join(f'0x{v:04X}' for v in chunk) + ',')
lines.append('};')
OUT.write_text('\n'.join(lines), encoding='utf-8')
print(f'Generated {OUT}')
