# Glossary: Slider Semantics

## Terminology

| Term | Meaning | Range | Example |
|------|---------|-------|---------|
| **Min** | Hardware minimum (fixed in HWconfig/Globals) | 0 | 0 |
| **Max** | Hardware maximum (fixed in HWconfig/Globals) | fixed | brightness: 242, volume: 0.47 |
| **Lo** | Current operational left boundary (from Globals) | 0-Max | brightness: 70, volume: 0.05 |
| **Hi** | Current operational right boundary (varies by shift/sensor) | Lo-Max | varies |
| **fraction** | Attenuation only (cannot amplify) | 0.0-1.0 | 0.5 = half, 1.0 = full |
| **multiplier** | Can attenuate or amplify | 0.0+ (no upper limit) | 0.5 = half, 1.4 = 140% |
| **shift** | Integer percentage adjustment | any int | -5, +3 |
| **pct** | Percentage value (0-100) | 0-100 | sliderPct, loPct, hiPct |
| **sliderPct** | Slider position as percentage | 0-100 | current brightness % |
| **loPct** | Lo as percentage of Max | 0-100 | (Lo / Max) × 100 |
| **hiPct** | Hi as percentage of Max | 0-100 | (Hi / Max) × 100 |
| **webShift** | User brightness multiplier from slider | 0.0+ | can be >1.0 to compensate other shifts |

### Fraction vs Multiplier

```
fraction:     0.0 .. 1.0  (attenuate only, never amplify)
multiplier:   0.0 .. ∞    (can attenuate OR amplify)

Example webShift as multiplier:
- Other shifts bring brightness to 70%
- User wants 100% → webShift = 100/70 = 1.43
- webShift > 1.0 compensates other shifts
```

### Sensor Ranges (consistent Lo..Hi pairs)

| Sensor | Min | Max | Example |
|--------|-----|-----|---------|
| **lux** | luxMin (0) | luxMax (800) | ambient light |

### Shift → Multiplier Conversion

```
multiplier = 1 + (shift / 100)

shift = +5  → multiplier = 1.05
shift = -3  → multiplier = 0.97
shift = 0   → multiplier = 1.00
```

### Lux to Shift Mapping

```
luxShift = map(lux, luxMin, luxMax, shiftLo, shiftHi)
luxMultiplier = 1 + (luxShift / 100)

Example with shiftLo=-10, shiftHi=+10:
lux=0   → luxShift=-10 → multiplier=0.90
lux=400 → luxShift=0   → multiplier=1.00
lux=800 → luxShift=+10 → multiplier=1.10
```

### Banned synonyms (use the term above instead)
- ~~mult~~ → use **multiplier**
- ~~factor~~ → use **fraction** (if 0.0-1.0) or **multiplier** (if can exceed 1.0)
- ~~bright~~ → use **brightness** (full word)
- ~~gain~~ → use **volume** (except for I2S hardware registers)
- ~~offset~~ → use **shift** (for percentage adjustments)
- ~~modifier~~ → use **fraction** or **multiplier** (be explicit about range)
- ~~thumbPct~~ → use **sliderPct** (slider position percentage)

## Mapping Formula

```
sliderPct = map(shiftedHi, Globals::brightnessLo, Globals::brightnessHi, Globals::loPct, Globals::hiPct)
brightness = map(sliderPct, Globals::loPct, Globals::hiPct, Globals::brightnessLo, Globals::brightnessHi)
```

## SSE Fields

### Brightness
| Field | Type | Source |
|-------|------|--------|
| `userBrightness` | 0-255 | User's fraction × MAX_BRIGHTNESS |
| `brightnessLo` | 0-255 | Globals::luxMinBase |
| `brightnessHi` | 0-255 | No sensor: MAX_BRIGHTNESS, With sensor: lux-computed |
| `brightnessMax` | 0-255 | MAX_BRIGHTNESS (250) |

### Audio
| Field | Type | Source |
|-------|------|--------|
| `userVolume` | 0.0-1.0 | User's volume fraction |
| `audioLo` | 0.0-1.0 | Globals::audioLo |
| `audioHi` | 0.0-1.0 | getBaseGain() (time-shifted) |
| `audioMax` | 0.0-1.0 | MAX_AUDIO_VOLUME (0.47) |

## JS Variables (audio.js, brightness.js)

| Variable | Meaning |
|----------|---------|
| `loPct` | Left grey zone boundary (%) |
| `hiPct` | Right grey zone boundary (%) |
| `modifier` | User's fraction (0.0-1.0) — pending rename, see Proposed Renames |
| `thumbPct` | Slider position (%) — pending rename to `sliderPct`, see Proposed Renames |

## Visual

```
Brightness: [░░░░░░░■■■■■■■■░░░░░]
             0%   loPct    hiPct  100%
                   28%     100%

Audio:      [░░■■■■■■■■■■░░░░░░░░]
             0% loPct    hiPct  100%
                ~11%     ~74%
```

## Proposed Renames (TODO)

| Current | Proposed | Reason |
|---------|----------|--------|
| `MAX_AUDIO_VOLUME` | `MAX_VOLUME` | Consistent with volume terminology |
| `Globals::baseGain` | `Globals::volumeUnshiftedHi` | Hi value before shifts |
| `Globals::maxAudioVolume` | **REMOVE** | Use `MAX_VOLUME` only |
| `g_baseGain` (state) | `g_volumeShiftedHi` | Hi boundary after shifts |
| `getBaseGain()` | `getVolumeShiftedHi()` | Returns shifted Hi |
| `setBaseGain()` | `setVolumeShiftedHi()` | Sets shifted Hi |
| `g_webAudioLevel` | `g_volumeWebFraction` | User's volume fraction from web |
| `getWebAudioLevel()` | `getVolumeWebFraction()` | Clear meaning |
| `setWebAudioLevel()` | `setVolumeWebFraction()` | Clear meaning |
| `gain` (general) | `volume` | Use "gain" only for I2S hardware |
| `modifier` (JS) | `brightnessFraction` | Align with fraction terminology |
| `thumbPct` (JS) | `sliderPct` | Align with sliderPct terminology |
