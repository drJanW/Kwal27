# Glossary: Slider Semantics

> Version: 260319A | Updated: 2026-03-19

## Terminology

| Term | Meaning | Range | Example |
|------|---------|-------|---------|
| **Min** | Hardware minimum (fixed in HWconfig/Globals) | 0 | 0 |
| **Max** | Hardware maximum (fixed in HWconfig/Globals) | fixed | brightness: 242, volume: 0.47 |
| **Lo** | Current operational left boundary (from Globals) | 0-Max | brightness: 70, volume: 0.05 |
| **Hi** | Current operational right boundary (varies by shift/sensor) | Lo-Max | varies |
| **fraction** | Attenuation only (cannot amplify) | 0.0-1.0 | 0.5 = half, 1.0 = full |
| **multiplier** | Can attenuate or amplify | 0.0+ (no upper limit) | 0.5 = half, 1.4 = 140% |
| **swing** | Bipolar proportional value (centered on 0) | -1.0 to +1.0 | temperatureSwing |
| **shift** | Integer percentage adjustment | any int | -5, +3 |
| **pct** | Percentage value (0-100) | 0-100 | sliderPct, loPct, hiPct |
| **sliderPct** | Slider position as percentage | 0-100 | current brightness % |
| **loPct** | Lo as percentage of Max | 0-100 | (Lo / Max) x 100 |
| **hiPct** | Hi as percentage of Max | 0-100 | (Hi / Max) x 100 |
| **webMultiplier** | User brightness/volume multiplier from slider | 0.0+ | can be >1.0 to compensate |

### Fraction vs Multiplier

```
fraction:     0.0 .. 1.0  (attenuate only, never amplify)
multiplier:   0.0 .. inf  (can attenuate OR amplify)

Example webMultiplier as multiplier:
- Other shifts bring brightness to 70%
- User wants 100% -> webMultiplier = 100/70 = 1.43
- webMultiplier > 1.0 compensates other shifts
```

### Shift to Multiplier Conversion

```
multiplier = 1 + (shift / 100)

shift = +5  -> multiplier = 1.05
shift = -3  -> multiplier = 0.97
shift = 0   -> multiplier = 1.00
```

### Banned synonyms (use the term above instead)
- ~~mult~~ -> use **multiplier**
- ~~factor~~ -> use **fraction** (if 0.0-1.0) or **multiplier** (if can exceed 1.0)
- ~~bright~~ -> use **brightness** (full word)
- ~~gain~~ -> use **volume** (except for I2S hardware registers)
- ~~offset~~ -> use **shift** (for percentage adjustments)
- ~~modifier~~ -> use **fraction** or **multiplier** (be explicit about range)
- ~~thumbPct~~ -> use **sliderPct**

## Mapping Formula

```
sliderPct = map(shiftedHi, brightnessLo, brightnessHi, loPct, hiPct)
brightness = map(sliderPct, loPct, hiPct, brightnessLo, brightnessHi)
```

## SSE Fields

### Brightness (state event)

| SSE Field | Type | Source |
|-----------|------|--------|
| `sliderPct` | 0-100 | Current brightness slider position |
| `brightnessLo` | 0-255 | Globals::brightnessLo |
| `brightnessHi` | 0-255 | Shift-adjusted ceiling |
| `brightnessMax` | 0-255 | MAX_BRIGHTNESS (242) |

### Audio (state event)

| SSE Field | Type | Source |
|-----------|------|--------|
| `audioSliderPct` | 0-100 | getAudioSliderPct() |
| `volumeLo` | 0.0-1.0 | Globals::volumeLo (0.05) |
| `volumeHi` | 0.0-1.0 | getVolumeShiftedHi() (shift ceiling) |
| `volumeMax` | 0.0-1.0 | MAX_VOLUME (0.47) |

### Other SSE state fields

| SSE Field | Type | Source |
|-----------|------|--------|
| `patternId` | string | Active pattern ID |
| `patternLabel` | string | Active pattern label |
| `colorId` | string | Active colors ID |
| `colorLabel` | string | Active colors label |
| `fragment` | object | {dir, file, score, durationMs, boxName} |
| `silence` | bool | Silence mode active |
| `speakMin` | int | TTS interval minutes |
| `fragMin` | int | Fragment interval minutes |
| `durMin` | uint32 | Minimum fragment duration ms |
| `tvMode` | bool | TV simulator active |
| `hasLuxSensor` | bool | Lux sensor present |
| `sleepArmed` | bool | Sleep timer armed |

## JS Variables

| JS Variable | File | Meaning |
|-------------|------|---------|
| `sliderPct` | brightness.js | Brightness slider position (%) |
| `brightnessLo` | brightness.js | Left grey zone boundary |
| `brightnessHi` | brightness.js | Right grey zone boundary |
| `brightnessMax` | brightness.js | Hardware maximum |
| `audioSliderPct` | audio.js | Audio slider position (%) |
| `volumeLo` | audio.js | Audio left boundary |
| `volumeHi` | audio.js | Audio right boundary |
| `volumeMax` | audio.js | Audio hardware maximum |

## Visual

```
Brightness: [##########============########]
             0%   loPct    hiPct  100%
                   28%     100%

Audio:      [####============##############]
             0% loPct    hiPct  100%
                ~11%     ~74%
```

## Completed Renames

All proposed renames from earlier versions have been implemented:

| Old Name | New Name | Status |
|----------|----------|--------|
| `MAX_AUDIO_VOLUME` | `MAX_VOLUME` | DONE |
| `Globals::baseGain` | `Globals::volumeHi` | DONE |
| `Globals::maxAudioVolume` | REMOVED | DONE |
| `g_baseGain` (state) | `g_volumeShiftedHi` | DONE |
| `getBaseGain()` | `getVolumeShiftedHi()` | DONE |
| `setBaseGain()` | `setVolumeShiftedHi()` | DONE |
| `g_webAudioLevel` | `g_volumeWebMultiplier` | DONE |
| `getWebAudioLevel()` | `getVolumeWebMultiplier()` | DONE |
| `setWebAudioLevel()` | `setVolumeWebMultiplier()` | DONE |
| `modifier` (JS) | removed / inlined | DONE |
| `thumbPct` (JS) | `sliderPct` | DONE |
