
# MC DSi Prototype

This is a minimal Nintendo DSi port of Minecaft Legacy Edition

## Requirements

- devkitPro
- devkitARM
- libnds
- ndstool
- On newer setups, Calico headers may also be required
- Python3
- Python3-Pillow

## Build

Simple. Run buildrom.sh.

## Controls

- D-Pad: move
- A/B/X/Y: look
- L/R: Break/Place Blocks
- Start: Pause
- Select: Jump

## Notes

This package was patched to include the Calico header root automatically when present:

- `$(DEVKITPRO)/calico/include`

That fixes builds where `libnds/include/nds.h` includes `<calico.h>`.

## Texture workflow

This DSi port now auto-generates `source/texture_data.cpp` from the PNG files found in `textures/` every time you run `make`.

Expected filenames for the current placeable blocks:

- `grass_top.png`
- `dirt.png`
- `stone.png`
- `wood_top.png`
- `wood_side.png`
- `leaves.png`
- `sand.png`
- `water.png`
- `cobblestone.png`
- `planks.png`
- `brick.png`
- `glass.png`
- `birch_log_top.png`
- `birch_log_side.png`
- `spruce_log_top.png`
- `spruce_log_side.png`
- `jungle_log_top.png`
- `jungle_log_side.png`
- `birch_leaves.png`
- `spruce_leaves.png`
- `jungle_leaves.png`
- `birch_planks.png`
- `spruce_planks.png`
- `jungle_planks.png`
- `sandstone.png`
- `obsidian.png`
- `gravel.png`
- `bookshelf.png`
- `white_wool.png`
- `gold_block.png`
- `iron_block.png`


Notes:

- PNGs are resized to 8x8 with nearest-neighbor during the build if needed.
- If a PNG is missing, the build falls back to the embedded placeholder texture for that block.
- When new block types are added later, add the new filename mapping in `tools/generate_texture_data.py` and drop the PNG into `textures/`.
