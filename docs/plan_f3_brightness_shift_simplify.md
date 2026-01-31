# Plan F3: Simplify Brightness Shifts (HSV V → FastLED)

> Version: 260101A | Status: DRAFT

## Problem

Current brightness shift via HSV V-component:
1. `colorsShifts.csv` has `colorA.value` and `colorB.value` columns
2. Both are ALWAYS identical in all entries (per-color control unused)
3. Requires RGB→HSV→modify V→HSV→RGB conversion (CPU overhead)
4. Also has dead `colorA.brightness` / `colorB.brightness` columns (never used)

## Proposed Solution

Replace per-color HSV V shifts with global FastLED brightness shift.

### CSV Change

**Before:**
```csv
status;colorA.brightness;colorA.hue;colorA.saturation;colorA.value;colorB.brightness;colorB.hue;colorB.saturation;colorB.value
isNight;-50;0;0;-50;-50;0;0;-50
```

**After:**
```csv
status;colorA.hue;colorA.saturation;colorB.hue;colorB.saturation;globalBrightness
isNight;0;0;0;0;-50
```

### Code Changes

#### 1. ContextStatus.h - Remove unused enums

```cpp
// REMOVE:
enum ColorParam : uint8_t {
    COLOR_A_HUE = 0,
    COLOR_A_SAT,
    COLOR_A_BRIGHT,    // REMOVE - dead code
    COLOR_A_VALUE,     // REMOVE - replaced by globalBrightness
    COLOR_B_HUE,
    COLOR_B_SAT,
    COLOR_B_BRIGHT,    // REMOVE - dead code
    COLOR_B_VALUE,     // REMOVE - replaced by globalBrightness
    COLOR_PARAM_COUNT
};

// KEEP (simplified):
enum ColorParam : uint8_t {
    COLOR_A_HUE = 0,
    COLOR_A_SAT,
    COLOR_B_HUE,
    COLOR_B_SAT,
    GLOBAL_BRIGHTNESS,  // NEW
    COLOR_PARAM_COUNT
};
```

#### 2. ShiftStore.cpp - Update parser

```cpp
// REMOVE:
if (key == "colorA.brightness") { out = COLOR_A_BRIGHT; return true; }
if (key == "colorA.value") { out = COLOR_A_VALUE; return true; }
if (key == "colorB.brightness") { out = COLOR_B_BRIGHT; return true; }
if (key == "colorB.value") { out = COLOR_B_VALUE; return true; }

// ADD:
if (key == "globalBrightness") { out = GLOBAL_BRIGHTNESS; return true; }
```

#### 3. LightRun.cpp - Apply via FastLED

**Before (HSV V shift):**
```cpp
int aValShift = multToShift(colorMults[COLOR_A_VALUE], 255.0f);
if (aHueShift != 0 || aSatShift != 0 || aValShift != 0) {
    colorA = shiftColorHSV(colorA, aHueShift, aSatShift, aValShift);
}
```

**After (FastLED brightness):**
```cpp
// Apply H/S shifts only (no V)
if (aHueShift != 0 || aSatShift != 0) {
    colorA = shiftColorHS(colorA, aHueShift, aSatShift);
}

// Apply global brightness shift via FastLED
float brightnessMult = colorMults[GLOBAL_BRIGHTNESS];
applyBrightnessShift(brightnessMult);
```

#### 4. New function in LightManager

```cpp
void applyBrightnessShift(float mult) {
    // mult: 1.0 = no change, 0.5 = half brightness, 1.5 = 50% brighter
    uint8_t base = getBrightnessShiftedHi();
    uint8_t shifted = static_cast<uint8_t>(constrain(base * mult, 0, 255));
    setBrightnessShiftedHi(shifted);
}
```

### Pseudo-code Flow

```
1. Boot: Load colorsShifts.csv
   - Parse globalBrightness column
   - Store in ShiftStore

2. Context change (time/weather/etc):
   - ContextFlags::getFullContextBits() returns active statuses
   - ShiftStore::computeColorMultipliers() calculates:
     - H/S multipliers for colorA/B
     - globalBrightness multiplier (product of all active shifts)

3. LightRun::applyToLights():
   - Get base colors from ColorsStore
   - Apply H/S shifts via shiftColorHS() (simplified, no V)
   - Get brightness mult from colorMults[GLOBAL_BRIGHTNESS]
   - Call applyBrightnessShift(mult)
     - Modifies brightnessShiftedHi
     - applyFinalBrightness() will use this in next frame

4. applyFinalBrightness() (already exists):
   - Uses brightnessShiftedHi as Hi boundary
   - WebGUI modifier scales within Lo..Hi range
   - FastLED.setBrightness(finalBrightness)
```

### Benefits

1. Simpler code - no RGB↔HSV conversion for brightness
2. Less CPU cycles per frame
3. Cleaner CSV - 5 columns instead of 9
4. Removes dead code (colorA/B.brightness)
5. Single brightness control path (FastLED only)

### Risks

1. Loss of per-color brightness control (currently unused anyway)
2. CSV migration required
3. Needs careful testing of shift accumulation

### Migration

1. Create new CSV format
2. Convert existing values: take `colorA.value` as `globalBrightness`
3. Update parser
4. Update LightRun
5. Test all context combinations

## Files to Change

| File | Change |
|------|--------|
| `excel/colorsShifts.csv` | Remove 4 columns, add globalBrightness |
| `sdroot/colorsShifts.csv` | Same |
| `lib/ContextManager/ContextStatus.h` | Simplify ColorParam enum |
| `lib/ConductManager/Light/ShiftStore.cpp` | Update parser |
| `lib/ConductManager/Light/LightRun.cpp` | Remove V shift, add brightness shift call |
| `lib/LightManager/LightManager.cpp` | Add applyBrightnessShift() |
| `lib/LightManager/LightManager.h` | Declare applyBrightnessShift() |

## Status

- [ ] Plan reviewed
- [ ] CSV migrated
- [ ] Code updated
- [ ] Tested
