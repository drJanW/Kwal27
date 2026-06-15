# LightController

> Version: 260614B | Updated: 2026-06-14

LED control interface for 160 WS2812B LEDs via FastLED, with pattern playback, brightness management, and TV simulator mode.

## Files

| File | Version | Purpose |
|------|---------|---------|
| `LightController.h/.cpp` | 260607A | Main LED control: brightness, patterns, color cycles, show playback |
| `LightManager.cpp` | — | Lifecycle management (implementation-only, no .h) |
| `LEDMap.h/.cpp` | 260227B | Physical LED strip mapping to logical (x,y) positions via `ledmap.bin` |
| `TvShow.h/.cpp` | 260307C | TV simulator renderer — 6 ring zones matching PMMA circles |
| `RingShow.h/.cpp` | 260615A | Ring renderer dispatch for any pattern with `pattern_type` set; replaces Spectrum |

## Architecture

LightController is a **Controller** layer module — it owns the FastLED hardware driver.
Pattern orchestration lives in `lib/RunManager/Light/` (LightRun, LightPolicy, LightDirector).

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

// TvShow.h — TV simulator
void setTvZoneTargets(const TvZoneTarget targets[TV_ZONES]);
void updateTvShow();
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

## RingShow — Universal Ring Renderer (v260615A)

Replaces the old Spectrum module (removed). **Any pattern** can use 6-ring rendering
by setting `pattern_type` in `light_patterns.csv` (columns 18-19):

- `pattern_type` — selects the ring renderer: `Rainbow`, `Blended`, `Static`
- `ring_colors` — semicolon-separated `r,g,b` triples for `Static` renderer (6 rings)

Leave both fields empty for traditional CircleShow rendering.

**Renderers:**

| Type | Behavior | Uses RGB1/RGB2? | Uses ringColors? |
|------|----------|-----------------|------------------|
| `Rainbow` | User-chosen gradient spread across 6 rings | Yes | No |
| `Blended` | Same layout as Rainbow, alternate name | Yes | No |
| `Static` | Fixed colors per ring from CSV | No | Yes |

**Dispatch:** `updateLightController()` checks `!patternType.isEmpty()`, 
so any pattern with a `pattern_type` value triggers RingShow. The assigned color set always
provides RGB1→RGB2 via `generateColorGradient()`. Patterns never pick their own colors.
- `Rainbow`/`Blended` → `generateColorGradient(RGB1, RGB2)` 
- `Static` → no gradient needed (uses `ring_colors` CSV field)

## TvShow — TV Simulator (v260307C)

Ring-based TV simulator for the 6 concentric PMMA ring zones:
- **6 zones** (0-5, innermost to outermost): 8→16→24→32→36→44 LEDs per ring (160 total)
- `setTvZoneTargets(targets[6])` — set target colors per ring
- `updateTvShow()` — lerp-based smooth rendering
- Controlled via `tv.js` in the WebGUI

## Ambient Lux Coordination

- **Sensor**: VEML7700 (replaced BH1750 in v260215+)
- RunManager `requestLuxMeasurement()` turns LEDs off, waits ~120 ms, triggers `SensorController::performLuxMeasurement()`, then calls `updateBaseBrightness()` before restoring LEDs
- **LuxCalibration** (v260319A): Gauss-Newton fit engine. Model: `brightness = brMax × (1 - exp(-luxRate × lux))`. User-trainable via WebGUI calibration panel. See `lib/RunManager/Light/LuxCalibration.h`