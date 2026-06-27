# LightController

> Version: 260615C | Updated: 2026-06-15

LED control interface for 160 WS2812B LEDs via FastLED, with pattern playback, brightness management, TV simulator mode, and demo mode.

## Files

| File | Version | Purpose |
|------|---------|---------|
| | `LightController.h/.cpp` | 260615C | Main LED control: brightness, patterns, color cycles, show playback |
| | `LightManager.cpp` | — | Lifecycle management (implementation-only, no .h) |
| | `LEDMap.h/.cpp` | 260227B | Physical LED strip mapping to logical (x,y) positions via `ledmap.bin` |
| | `TvShow.h/.cpp` | 260307C | TV simulator renderer — 6 ring zones matching PMMA circles |
| | `RingShow.h/.cpp` | 260615C | Unified 6-ring renderer: gradient scrolling + lerp targets (merged RingRenderer) |

## Architecture

LightController is a **Controller** layer module — it owns the FastLED hardware driver.
Pattern orchestration lives in `lib/RunManager/Light/` (LightRun, LightPolicy, LightDirector).

**Background suspension** (v260615C): TV mode and demo mode are "foreground" modes
that suspend normal ring activities. A unified gate `Globals::isBackgroundSuspended()`
(replaces separate `tvMode`/`demoActive` checks) blocks:
- Light: `updateLightController()`, `applyBrightness()`, lux measurements, PNF calibration, color/pattern change callbacks
- Audio: `cb_sayTime()`, `cb_sayRTCtemperature()`, `cb_playFragment()`
- Calendar: reloads

Both modes exit via reboot (5s delay).

## API

```cpp
// LightController.h — main show control
void PlayLightShow(const LightShowParams& params);
LightShowParams MakeSolidParams(CRGB color);
void updateLightController();        // Called from main loop

// Brightness
float getWebMultiplier();
void setWebMultiplier(float value);
int getSliderPct();
uint8_t getBrightnessShiftedHi();
void setBrightnessShiftedHi(float value);
uint8_t getBrightnessBaseHi();
void setBrightnessBaseHi(uint8_t value);
void applyBrightness();              // Runs every 50ms

// Color/brightness cycles
void cb_colorCycle();
void cb_brightCycle();
void generateColorGradient(const CRGB& colorA, const CRGB& colorB, CRGB* gradient, int n);

// LEDMap.h — position mapping
LEDPos getLEDPos(int index);
bool loadLEDMapFromSD(const char* path);

// RingShow.h — unified ring renderer
void setRingTargets(const RingTarget targets[RING_COUNT]);  // TV simulator, demo RingScene
void renderRings();                                           // lerp render one frame
void updateRingShow(const LightShowParams& params, const CRGB* gradient, uint8_t colorPhase, uint8_t maxBri);
```

## Light Shows

Each light show uses:
- `LightShowParams` — universal struct (colors, cycles, type)
- Show-specific extra params struct (defined in `LightController.h`)
- Play entry point in `LightController.cpp`
- No own timers — all timing via central cycles

Adding a new show:
1. Define `ExtraXXParams` struct in `LightController.h`
2. Add to `enum LightShow`
3. Add play entry point and update switch/case in `updateLightController()` and `PlayLightShow()`

## RingShow — Unified 6-Ring Renderer (v260615C)

**Merged**: Former `RingShow` (pattern dispatch) + `RingRenderer` (lerp targets) are now
one module. The old `RingRenderer.h/.cpp` and `Spectrum.h/.cpp` are removed.

**Any pattern** can use 6-ring rendering by setting `pattern_type` in `light_patterns.csv`:

- `pattern_type` — selects the ring renderer: `Gradient` or `Static`
- `ring_colors` — semicolon-separated `r,g,b` triples for `Static` renderer (6 rings)

Leave both fields empty for traditional CircleShow rendering.

**Renderers:**

| Type | Behavior | Uses RGB1/RGB2? | Uses ringColors? |
|------|----------|-----------------|------------------|
| `Gradient` | User-chosen gradient scrolls across 6 rings | Yes | No |
| `Static` | Fixed colors per ring from CSV | No | Yes |

**Dispatch:** `updateLightController()` checks `!patternType.isEmpty()`, 
so any pattern with a `pattern_type` value triggers `updateRingShow()`. The assigned color
set always provides RGB1→RGB2 via `generateColorGradient()`.
- `Gradient` → `generateColorGradient(RGB1, RGB2)` — scrolls via `colorPhase`
- `Static` → no gradient needed (uses `ring_colors` CSV field)

**Lerp targets**: `setRingTargets()` / `renderRings()` are also exported for direct
per-ring control (TV simulator, demo RingScene). These bypass `updateLightController()`
entirely — they write to `leds[]` and call `FastLED.show()` directly.

## Radial Patterns / Angle Blend (v260615C)

**New in `light_patterns.csv`** — columns 19–20:
- `angle_weight` (float 0..1): 0 = pure distance blend (classic), 1 = pure angle blend
- `petal_count` (uint8_t): angular fold count for repeating patterns (1 = no repetition, 6 = 6-fold symmetry)

These columns **extend the existing CircleShow renderer** — no new renderer or `pattern_type` 
is needed. When `angle_weight > 0.0` and `petal_count > 0`, the per-LED blend switches from 
pure distance to a lerp between distance and angle blends.

### Angle Blend Math

For each LED at polar coordinate `(r, θ)`:
1. Normalize θ to 0..1
2. Fold into `petal_count` segments: `fmod(θ × petalCount, 1)`
3. Compute distance from each petal center (0.5): `|folded - 0.5| × 2` → 0 at center, 1 at edge
4. Blend this against distance blend using `angleWeight` as lerp factor

### Use Cases

| angleWeight | petalCount | Effect |
|-------------|------------|--------|
| 0.0 | 1 | Classic concentric rings (distance only) |
| 0.5 | 4 | 4-fold flower petals fading toward center |
| 1.0 | 6 | Pure angular — 6 "pie wedges" (mandala) |
| 1.0 | 1 | Lighthouse / rotating spotlight (add radius_osc + x_amp/y_amp for motion) |
| 0.3 | 8 | 8-pointed star with soft distance falloff |

Combining with existing parameters:
- `radius_osc` + `angleWeight=1.0` + `petalCount=1` = rotating lighthouse beam
- `window_width` influences angular softness (wider = softer angle transitions)
- `center_x`/`center_y` offset the center of both distance AND angle calculations

### Parameters in `LightShowParams`

```cpp
struct LightShowParams {
    // ... existing fields unchanged ...
    float angleWeight  = 0.0f;   // 0 = pure distance, 1 = pure angle
    uint8_t petalCount  = 1;     // angular fold count (must be > 0 for angle blend)
};
```

### CSV Format

Existing patterns gain `;0;1` at end (distance-only, 1 petal = no folding).
New radial pattern example (pattern 33 in CSV):
```
33;Radial Mandala;18;16;12.000;12;0.600;0.000;0.000;28.000;36;4.000;0.000;0.000;24;22;0.0000;;;1.000;6
```

## Ambient Lux Coordination

- **Sensor**: VEML7700 (replaced BH1750 in v260215+)
- RunManager `requestLuxMeasurement()` turns LEDs off, waits ~120 ms, triggers `SensorController::performLuxMeasurement()`, then calls `updateBaseBrightness()` before restoring LEDs
- **LuxCalibration** (v260319A): Gauss-Newton fit engine. Model: `brightness = brMax × (1 - exp(-luxRate × lux))`. User-trainable via WebGUI calibration panel. See `lib/RunManager/Light/LuxCalibration.h`