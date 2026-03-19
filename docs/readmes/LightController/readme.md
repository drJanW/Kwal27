# LightController

> Version: 260319A | Updated: 2026-03-19

LED control interface for 160 WS2812B LEDs via FastLED, with pattern playback, brightness management, and TV simulator mode.

## Files

| File | Version | Purpose |
|------|---------|---------|
| `LightController.h/.cpp` | 260307A | Main LED control: brightness, patterns, color cycles, show playback |
| `LightManager.cpp` | — | Lifecycle management (implementation-only, no .h) |
| `LEDMap.h/.cpp` | 260227B | Physical LED strip mapping to logical (x,y) positions via `ledmap.bin` |
| `TvShow.h/.cpp` | 260307C | TV simulator renderer — 6 ring zones matching PMMA circles |

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