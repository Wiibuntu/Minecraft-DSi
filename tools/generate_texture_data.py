#!/usr/bin/env python3
from __future__ import annotations
from pathlib import Path

TEX_SIZE = 8
ROOT = Path(__file__).resolve().parent.parent
TEXTURES_DIR = ROOT / 'textures'
OUTPUT = ROOT / 'source' / 'texture_data.cpp'

TEXTURE_SPECS = [
    ('TEX_GRASS_TOP', 'grass_top.png'),
    ('TEX_DIRT', 'dirt.png'),
    ('TEX_STONE', 'stone.png'),
    ('TEX_WOOD_TOP', 'wood_top.png'),
    ('TEX_WOOD_SIDE', 'wood_side.png'),
    ('TEX_LEAVES', 'leaves.png'),
    ('TEX_SAND', 'sand.png'),
    ('TEX_WATER', 'water.png'),
    ('TEX_COBBLE', 'cobblestone.png'),
    ('TEX_PLANKS', 'planks.png'),
    ('TEX_BRICK', 'brick.png'),
    ('TEX_GLASS', 'glass.png'),
    ('TEX_BIRCH_WOOD_TOP', 'birch_log_top.png'),
    ('TEX_BIRCH_WOOD_SIDE', 'birch_log_side.png'),
    ('TEX_SPRUCE_WOOD_TOP', 'spruce_log_top.png'),
    ('TEX_SPRUCE_WOOD_SIDE', 'spruce_log_side.png'),
    ('TEX_JUNGLE_WOOD_TOP', 'jungle_log_top.png'),
    ('TEX_JUNGLE_WOOD_SIDE', 'jungle_log_side.png'),
    ('TEX_BIRCH_LEAVES', 'birch_leaves.png'),
    ('TEX_SPRUCE_LEAVES', 'spruce_leaves.png'),
    ('TEX_JUNGLE_LEAVES', 'jungle_leaves.png'),
    ('TEX_BIRCH_PLANKS', 'birch_planks.png'),
    ('TEX_SPRUCE_PLANKS', 'spruce_planks.png'),
    ('TEX_JUNGLE_PLANKS', 'jungle_planks.png'),
    ('TEX_SANDSTONE', 'sandstone.png'),
    ('TEX_OBSIDIAN', 'obsidian.png'),
    ('TEX_GRAVEL', 'gravel.png'),
    ('TEX_BOOKSHELF', 'bookshelf.png'),
    ('TEX_WHITE_WOOL', 'white_wool.png'),
    ('TEX_GOLD_BLOCK', 'gold_block.png'),
    ('TEX_IRON_BLOCK', 'iron_block.png'),
    ('TEX_TORCH', 'torch.png'),
]

ITEM_TEXTURE_SPECS = [
    ('TEX_ITEM_APPLE', Path('items/apple.png')),
    ('TEX_ITEM_WOOD_PICKAXE', Path('items/wooden_pickaxe.png')),
    ('TEX_ITEM_STONE_PICKAXE', Path('items/stone_pickaxe.png')),
    ('TEX_ITEM_WOOD_AXE', Path('items/wooden_axe.png')),
    ('TEX_ITEM_WOOD_SHOVEL', Path('items/wooden_shovel.png')),
    ('TEX_ITEM_WOOD_SWORD', Path('items/wooden_sword.png')),
]

# Fallbacks intentionally simple for robustness on the DSi build.
FALLBACKS = {
    'TEX_GRASS_TOP': [0x91E9] * 64,
    'TEX_DIRT': [0xA170] * 64,
    'TEX_STONE': [0xBDEF] * 64,
    'TEX_WOOD_TOP': [0xA1D1] * 64,
    'TEX_WOOD_SIDE': [0x994D] * 64,
    'TEX_LEAVES': [0x89C3] * 64,
    'TEX_SAND': [0xD35B] * 64,
    'TEX_WATER': [0xFE2E] * 64,
    'TEX_COBBLE': [0xB16A] * 64,
    'TEX_PLANKS': [0xB22F] * 64,
    'TEX_BRICK': [0x98E9] * 64,
    'TEX_GLASS': [0xDEF7 if (i + (i // 8)) % 3 else 0 for i in range(64)],
    'TEX_BIRCH_WOOD_TOP': [0xA1D1] * 64,
    'TEX_BIRCH_WOOD_SIDE': [0x994D] * 64,
    'TEX_SPRUCE_WOOD_TOP': [0xA1D1] * 64,
    'TEX_SPRUCE_WOOD_SIDE': [0x994D] * 64,
    'TEX_JUNGLE_WOOD_TOP': [0xA1D1] * 64,
    'TEX_JUNGLE_WOOD_SIDE': [0x994D] * 64,
    'TEX_BIRCH_LEAVES': [0x89C3] * 64,
    'TEX_SPRUCE_LEAVES': [0x89C3] * 64,
    'TEX_JUNGLE_LEAVES': [0x89C3] * 64,
    'TEX_BIRCH_PLANKS': [0xB22F] * 64,
    'TEX_SPRUCE_PLANKS': [0xB22F] * 64,
    'TEX_JUNGLE_PLANKS': [0xB22F] * 64,
    'TEX_SANDSTONE': [0xD35B] * 64,
    'TEX_OBSIDIAN': [0x8421] * 64,
    'TEX_GRAVEL': [0xB16A] * 64,
    'TEX_BOOKSHELF': [0xB22F] * 64,
    'TEX_WHITE_WOOL': [0xDEF7] * 64,
    'TEX_GOLD_BLOCK': [0x02FF] * 64,
    'TEX_IRON_BLOCK': [0xD6B5] * 64,
    'TEX_TORCH': [0x03FF if (i % 8) < 2 else 0xA22F for i in range(64)],
}


def rgb5551(r: int, g: int, b: int, a: int) -> int:
    if a < 32:
        return 0
    return ((r >> 3) & 31) | (((g >> 3) & 31) << 5) | (((b >> 3) & 31) << 10) | 0x8000


def try_load_png(path: Path):
    try:
        from PIL import Image
    except Exception:
        return None, 'Pillow not installed; using fallback'

    if not path.exists():
        return None, 'missing'

    img = Image.open(path).convert('RGBA')
    if img.size != (TEX_SIZE, TEX_SIZE):
        img = img.resize((TEX_SIZE, TEX_SIZE), Image.NEAREST)

    out = []
    for y in range(TEX_SIZE):
        for x in range(TEX_SIZE):
            r, g, b, a = img.getpixel((x, y))
            out.append(rgb5551(r, g, b, a))
    return out, 'loaded'


def format_values(values: list[int]) -> str:
    lines = []
    for i in range(0, len(values), 8):
        chunk = values[i:i+8]
        lines.append('    ' + ', '.join(f'0x{v:04X}' for v in chunk) + ',')
    return '\n'.join(lines)


def main() -> int:
    generated = []
    status_lines = []

    for symbol, filename in TEXTURE_SPECS:
        values, status = try_load_png(TEXTURES_DIR / filename)
        if values is None:
            values = FALLBACKS[symbol]
        generated.append((symbol, filename, values))
        status_lines.append(f'// {symbol}: {filename} ({status})')

    for symbol, relpath in ITEM_TEXTURE_SPECS:
        values, status = try_load_png(TEXTURES_DIR / relpath)
        if values is None:
            values = [0] * (TEX_SIZE * TEX_SIZE)
        generated.append((symbol, relpath.as_posix(), values))
        status_lines.append(f'// {symbol}: {relpath.as_posix()} ({status})')

    cpp = ['#include "texture_data.h"', '']
    cpp.extend(status_lines)
    cpp.append('')
    for symbol, _, values in generated:
        cpp.append(f'const u16 {symbol}[TEX_SIZE * TEX_SIZE] = {{')
        cpp.append(format_values(values))
        cpp.append('};')
        cpp.append('')

    OUTPUT.write_text('\n'.join(cpp), encoding='utf-8')
    print(f'Generated {OUTPUT}')
    for line in status_lines:
        print(line[3:])
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
