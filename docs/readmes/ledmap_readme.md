# LED Map Pipeline

## Overview

`ledmap.bin` contains the physical (x,y) positions of all 160 WS2812B LEDs on the dome PCB.
The firmware uses these positions for distance-based pattern rendering (fade rings, gradients, wave effects).

## Source

**KiCAD PCB file**: `docs/kwal25.kicad_pcb`

The PCB has 160 WS2812B footprints labeled D1–D160, arranged in 6 concentric rings:

| Ring | LEDs       | Count | Radius (mm) |
|------|------------|-------|-------------|
| 1    | D1–D8      | 8     | ~13         |
| 2    | D9–D24     | 16    | ~23         |
| 3    | D25–D48    | 24    | ~33         |
| 4    | D49–D80    | 32    | ~42         |
| 5    | D81–D116   | 36    | ~51         |
| 6    | D117–D160  | 44    | ~59         |

## Coordinate Transform

KiCAD stores positions in absolute PCB coordinates with Y-axis pointing down.
The conversion to ledmap coordinates:

```
ledmap_x =  (pcb_x - 99.937)
ledmap_y = -(pcb_y - 100.001)
```

- **99.937, 100.001** = PCB center (dome origin in KiCAD space)
- **Y flip** = KiCAD Y-down → firmware Y-up

## Binary Format

`sdroot/ledmap.bin` — 1280 bytes total:

- 160 entries, sorted by reference designator (D1=index 0, D160=index 159)
- Each entry: 2× `float32` (little-endian) = 8 bytes
- Layout: `[x0, y0, x1, y1, ..., x159, y159]`

## Tools

| Script | Purpose |
|--------|---------|
| `tools/generate_ledmap.py` | Rebuild `ledmap.bin` from KiCAD PCB |
| `tools/verify_ledmap.py` | Verify existing `ledmap.bin` against PCB |

### Generate

```powershell
python tools/generate_ledmap.py          # overwrites sdroot/ledmap.bin
python tools/generate_ledmap.py --verify # compare without overwriting
```

### Verify

```powershell
python tools/verify_ledmap.py            # compare existing bin vs PCB
```

## Firmware Usage

- **Load**: `LightBoot.cpp` calls `loadLEDMapFromSD("/ledmap.bin")` at boot
- **Fallback**: if the file is missing, `buildFallbackLEDMap()` generates a simple circular layout (all 160 LEDs on one ring at radius ~12.6) — this is a rough approximation, not the real dome geometry
- **Consumer**: `LightController.cpp` calls `getLEDPos(i)` for each LED during pattern rendering
