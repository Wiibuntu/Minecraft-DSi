
# MC DSi Prototype

This is a minimal Nintendo DSi prototype scaffold intended as the first stage of a MinecraftConsoles-inspired handheld port.

## Requirements

- devkitPro
- devkitARM
- libnds
- ndstool
- On newer setups, Calico headers may also be required

## Build

```bash
export DEVKITPRO=/opt/devkitpro
export DEVKITARM=$DEVKITPRO/devkitARM
make SPECS='/opt/devkitpro/calico/share/ds9.specs'
```

If your setup does not use Calico specs, you can usually just run:

```bash
make
```

## Controls

- D-Pad: move
- A/B: move up/down
- L/R: yaw
- X/Y: pitch
- Start: toggle wireframe-ish overlay state placeholder

## Notes

This package was patched to include the Calico header root automatically when present:

- `$(DEVKITPRO)/calico/include`

That fixes builds where `libnds/include/nds.h` includes `<calico.h>`.

## March 2026 UI / block update

- Bottom screen map is now toggleable with a touch button in the top-right corner instead of always visible.
- Bottom screen now shows an expanded placeable block palette.
- Added additional basic block IDs for building: cobblestone, planks, brick, and glass.
- Uploaded font asset is staged at `assets/fonts/Minecraft.ttf` for future button/text work.
- Current button label rendering uses a lightweight built-in pixel routine for compatibility with the existing libnds-only build.


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

Notes:

- PNGs are resized to 8x8 with nearest-neighbor during the build if needed.
- If a PNG is missing, the build falls back to the embedded placeholder texture for that block.
- When new block types are added later, add the new filename mapping in `tools/generate_texture_data.py` and drop the PNG into `textures/`.
