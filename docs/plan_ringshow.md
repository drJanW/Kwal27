# Plan: RingShow — Formalized Ring-Based Pattern Renderer

> v260614A | 2026-06-14

## Motivation

Pattern #32 (Spectrum) is an architectural anomaly: the only pattern with its own `.h`/`.cpp` files, its own render loop, and an `if (id == 32)` early-return in `updateLightController()`. All other patterns (1-31) are data-driven via `showParams` → `generateColorGradient()` → per-LED distance math.

Simultaneously, `RingRenderer.h/.cpp` (née TvShow) already provides ring-based rendering with lerp-smoothing used by TV mode and Demo mode — but Spectrum duplicates its ring boundaries and rendering logic rather than sharing it.

This plan unifies the two: Spectrum becomes a proper RingShow that drives `RingRenderer`, ring boundaries exist once, and a `ring_colors` CSV column enables multicolored ring patterns referencing existing `light_colors.csv` entries.

---

## Architecture: Before → After

### Before
```
updateLightController()
├── if (tvMode) → renderRings() → FastLED.show() → return
├── if (id == 32) → build rainbow gradient → renderSpectrum() → FastLED.show() → return
└── CircleShow: generateColorGradient(RGB1,RGB2) → per-LED loop → FastLED.show()

RingRenderer   ← called by RunManager, DemoRun
Spectrum       ← called by updateLightController() (duplicate ring boundaries!)
```

### After
```
RingRenderer (single ring engine)
├── ringStart[7] = {0, 8, 24, 48, 80, 116, 160}  ← defined ONCE
├── setRingTargets()     ← color+brightness per ring
├── renderRings()        ← lerp + fill + FastLED.show()
└── Called by:
    ├── TV mode:       RunManager::enterTvMode() pushes scene targets
    ├── Demo:          DemoRun pushes demo scene targets
    └── RingShow:      updateLightController() pushes autonomous pattern targets

updateLightController()
├── if (tvMode) → renderRings() → return
├── if (patternType == RING_SHOW) → updateRingShow() → renderRings() → return
│   └── updateRingShow(): generate gradients from ColorsCatalog → setRingTargets()
└── CircleShow: unchanged

Spectrum.h/.cpp → DELETED (rendering logic now in updateRingShow() using RingRenderer)
```

---

## Phase 1: Unify Ring Rendering (Delete Spectrum Duplication)

### 1.1 Add `updateRingShow()` to LightController.cpp
- Load per-ring colors from `ringColors[6]` (color IDs from CSV)
- For each ring: `generateColorGradient(colorA, colorB, tempGrad, GRADIENT_SIZE)`
- Pick gradient color via `(colorPhase + ringOffset[z]) % GRADIENT_SIZE`
- Call `RingTarget` struct → `setRingTargets()`
- Uses existing `cb_colorCycle` timer — no new timers

### 1.2 Replace `if (id == 32)` with `if (patternType == RING_SHOW)`
- Pattern type determined by `patternId >= 32 && patternId < 40` (reserved range)
- Or explicit `pattern_type` column in CSV (TBD)

### 1.3 Delete Spectrum.h / Spectrum.cpp
- Remove from `lib/LightController/`
- Remove `#include "Spectrum.h"` from `LightController.cpp`
- Remove `extern CRGB leds[]` from Spectrum.cpp (it's already in LightController.cpp)
- Duplicate `ringStart` array dies with Spectrum

### 1.4 No changes to RingRenderer.h/.cpp
- RingRenderer API (`setRingTargets`, `renderRings`) stays identical
- `ringStart` boundaries stay where they are
- lerp logic, `renderRings()` body: zero changes

---

## Phase 2: CSV Schema Extension

### 2.1 New column: `ring_colors`
Comma-separated list of 6 `light_colors_id` values:
```
ring_colors=4,8,12,16,20,24
```
- Ring 0 (innermost, 8 LEDs) → color ID 4 (Royal Purple)
- Ring 1 (16 LEDs) → color ID 8 (Electric Blue)
- Ring 2 (24 LEDs) → color ID 12 (Lime Green)
- Ring 3 (32 LEDs) → color ID 16 (Lavender Mist)
- Ring 4 (36 LEDs) → color ID 20 (Turquoise Wave)
- Ring 5 (outermost, 44 LEDs) → color ID 24 (Indigo Night)

Parser must gracefully handle missing/empty values (default to pattern's active_color for all rings = backward compatible).

### 2.2 New column: `pattern_type`
Optional enum: `circle` (default) | `ringshow`

If `pattern_type=ringshow`:
- Uses `ring_colors` lookup (6 color IDs)
- Uses `ring_offsets` for phase staggering
- Uses `ring_brightness` for per-ring brightness scaling
- All existing params (RGB1, RGB2, fadeWidth, windowWidth, etc.) are **ignored** for RingShow

If absent → `circle` (all existing patterns 1-31 unchanged).

### 2.3 Pattern ID 32 in light_patterns.csv
```
pattern_id=32;pattern_type=ringshow;ring_colors=32,32,32,32,32,32;...
```
Spectrum color (ID 32) is already `#FF0000→#8000FF` — but Spectrum currently ignores it. The above config would produce 6 identical rings (matching old behavior), OR we assign different color IDs per ring for the new multicolor effect.

---

## Phase 3: Data Flow

### CSV Loading (`SDController` / `light_patterns.csv` parser)
```
Parse row → LightShowParams
├── If pattern_type == "ringshow":
│   ├── Parse ring_colors → uint8_t ringColorIds[6]
│   ├── Parse ring_offsets (optional) → uint8_t ringOffsets[6]
│   └── Parse ring_brightness (optional) → uint8_t ringBrightness[6]
└── Store in LightShowParams (or new RingShowParams struct)
```

### Render Chain (50ms tick)
```
updateLightController()
├── tvMode? → renderRings() → FastLED.show() → return
├── showParams.patternType == RING_SHOW?
│   ├── For each ring 0..5:
│   │   ├── Lookup color from ColorsCatalog[ringColorIds[z]]
│   │   ├── generateColorGradient(rgb1, rgb2, tempGrad, GRADIENT_SIZE)
│   │   ├── gradIdx = (colorPhase + ringOffsets[z]) % GRADIENT_SIZE
│   │   └── target.color = tempGrad[gradIdx] × ringBrightness[z]
│   ├── setRingTargets(targets)
│   └── renderRings() → FastLED.show() → return
└── CircleShow: unchanged
```

### ColorsCatalog Integration
- `ColorsCatalog` already loaded at boot from `light_colors.csv`
- `RingShow` does `ColorsCatalog::lookup(id)` for each of the 6 ring color IDs
- No new parsing, no new storage — one Catalog, two consumers (CircleShow, RingShow)

---

## Phase 4: File Changes

| File | Change |
|------|--------|
| `lib/LightController/Spectrum.h` | **DELETE** |
| `lib/LightController/Spectrum.cpp` | **DELETE** |
| `lib/LightController/LightController.cpp` | Add `updateRingShow()`, replace `if(id==32)` with `if(patternType==RING_SHOW)`, remove `#include "Spectrum.h"` |
| `lib/LightController/LightController.h` | Add `patternType` to `LightShowParams`, add `ringColorIds[6]`, `ringOffsets[6]`, `ringBrightness[6]` |
| `lib/LightController/readme.md` | Update file table (Spectrum removed, RingRenderer expanded) |
| `lib/SDController/` (CSV parser) | Add `ring_colors`, `pattern_type` parsing |
| `sdroot/light_patterns.csv` | Add `pattern_type` and `ring_colors` columns to header; update row 32 |
| `lib/Globals/Globals.h` | Bump `FIRMWARE_VERSION_CODE` 260614A → 260614B |
| `sdroot/version.txt` | Add firmware version line (SD format spec, no version field currently) |
| `lib/WebInterfaceController/` | No changes (no new web endpoints) |
| `sdroot/webgui-src/` | No changes (pattern editor needs only future work) |
| `sdroot/kwal.js` | No changes |
| `docs/readmes/LightController/readme.md` | Rewrite TvShow→RingRenderer section; add RingShow section |
| `docs/readmes/index.md` | s/TvShow renderer/RingRenderer/ |
| `docs/readmes/lib/readme.md` | s/TvShow (6-ring TV simulator)/RingRenderer (6-ring renderer + RingShow)/ |
| `docs/plan_ringshow.md` | This file |

---

## Phase 5: Default Ring Configs

| Ring index | Default offset | Default brightness | Rationale |
|------------|---------------|-------------------|-----------|
| 0 (inner) | 0 | 100% | Base reference |
| 1 | 42 | 100% | 42 × 6 = 252 ≈ full hue circle |
| 2 | 85 | 100% | 85 × 3 = 255 |
| 3 | 128 | 100% | Half-cycle offset |
| 4 | 170 | 100% | |
| 5 (outer) | 213 | 100% | |

These defaults produce Spectrum's current behavior (rainbow spread across 6 rings). The `ring_offsets` and `ring_brightness` CSV columns override.

---

## Phase 6: Migration

1. `light_patterns.csv` gets two new columns appended: `pattern_type`, `ring_colors`
2. Existing rows 1-31: `pattern_type` = (empty, defaults to "circle"), `ring_colors` = (empty)
3. Row 32: `pattern_type=ringshow`, `ring_colors=32,32,32,32,32,32` (backward compatible: all rings use Spectrum color)
4. User can later manually change `ring_colors` to e.g. `4,8,12,16,20,24` for multicolor
5. Future RingShow patterns: rows 33+ with `pattern_type=ringshow` and unique `ring_colors` combos

---

## Non-Goals (Explicitly Out of Scope)

- Per-ring scroll speed (still one global `colorPhase` per show — enough for v1)
- Per-ring radius oscillation (no motion params per ring)
- WebGUI pattern editor for RingShow (CSV editing only)
- Incorporating TV mode into RingShow (TV mode remains a system-wide mode, not a pattern)
- Merging `RingRenderer` into `LightController` (separation of concerns stays)

---

## Architectural Rules Compliance

| Rule | Compliance |
|------|-----------|
| Controller layer only touches FastLED | ✅ RingRenderer already does; RingShow code in LightController.cpp |
| TimerManager only for timing | ✅ Uses existing `cb_colorCycle`; no new timers |
| Web handlers: memory only | ✅ No new handlers |
| Shared state via atomics | ✅ ColorsCatalog lookup; no cross-controller calls |
| Naming canon | ✅ "Show" suffix established (TvShow→RingRenderer, LightShow, PlayLightShow) |
| Adding a show per readme | ✅ Replaces enum-less `if(id==32)` with proper pattern type dispatch |
| CSV backward compatibility | ✅ New columns at end; parser ignores unknown/missing |
| Versioning | ✅ `@version` on every touched file |

---

## Implementation Checklist

- [ ] Commit current state (version 260614A)
- [ ] Bump version to 260614B (`Globals.h` FIRMWARE_VERSION_CODE, `sdroot/version.txt`)
- [ ] Implement Phase 1: delete Spectrum, add `updateRingShow()`, switch dispatch
- [ ] Implement Phase 2: CSV parser extension for `ring_colors`/`pattern_type`
- [ ] Update `light_patterns.csv`: add columns, configure row 32
- [ ] Update `docs/readmes/LightController/readme.md`: rename TvShow→RingRenderer, add RingShow section
- [ ] Update `docs/readmes/index.md`: rename TvShow→RingRenderer
- [ ] Update `docs/readmes/lib/readme.md`: rename TvShow→RingRenderer
- [ ] Test: Spectrum behavior unchanged with default config
- [ ] Test: multicolor rings with custom `ring_colors`
