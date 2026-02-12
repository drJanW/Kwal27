# Audio Volume Semantics

> Version: 260207F | Updated: 2026-02-07

This document defines the exact meaning of every audio volume value in the system.

## The Volume Chain

Final output volume = `baseVolume × webAudioLevel × fadeFraction`

Where:
- `baseVolume` = system-controlled ceiling (shift-adjusted)
- `webAudioLevel` = user's web slider fraction (0.0-1.0)
- `fadeFraction` = transient fade envelope (0.0-1.0)

## Value Definitions

### Hardware Constants

| Name | Location | Value | Meaning |
|------|----------|-------|---------|
| `MAX_AUDIO_VOLUME` | HWconfig.h | 0.47f | Absolute hardware maximum. Speaker/amp limit. NEVER exceeded. |

### Runtime Values

| Name | Location | Default | Meaning |
|------|----------|---------|---------|
| `Globals::maxAudioVolume` | Globals.h / globals.csv | 0.5f (code) or 0.37 (csv) | Master ceiling from config. Used by shift calculation. |
| `Globals::baseGain` | Globals.h / globals.csv | 0.37f | Initial base volume before any shifts. |
| `Globals::audioLo` | Globals.h | 0.05f | Silence guard. Minimum playback level to avoid near-zero. |

### State Variables (AudioState.cpp)

| Name | Getter/Setter | Meaning |
|------|---------------|---------|
| `g_baseGain` | `getVolumeShiftedHi()` / `setVolumeShiftedHi()` | Current ceiling after shift application. Initialized from `Globals::volumeBaseHi`, updated by `AudioRun::applyVolumeShift()`. |
| `g_webAudioLevel` | `getVolumeWebMultiplier()` / `setVolumeWebMultiplier()` | User's multiplier from web slider. 1.0 = neutral, can be >1.0 to compensate shifts. |

**Note:** Internal variable names `g_baseGain` and `g_webAudioLevel` have been renamed to `g_volumeShiftedHi` and `g_volumeWebMultiplier`. Getter/setter names are current. `Globals::baseGain` pending rename to `Globals::volumeBaseHi` — see [glossary_slider_semantics.md](glossary_slider_semantics.md) Proposed Renames.

## Shift Application Flow

```
AudioShiftTable::getVolumeMultiplier(statusBits)
    → returns 0.0-1.0+ (can exceed 1.0 for boost)
    
AudioRun::applyVolumeShift(statusBits)
    → shiftedHi = volumeMultiplier × MAX_VOLUME
    → clamp to [0, MAX_VOLUME]
    → setVolumeShiftedHi(shiftedHi)
```

Log: `[AudioRun] Volume shift: 0.50 (mult=1.02, status=0x...)`
- `0.50` = shiftedHi stored in volumeShiftedHi
- `1.02` = raw volumeMultiplier from shift (>1.0 means boost)

## WebGUI Slider Semantics

The slider shows a percentage where:
- **0%** = audioLo (silence guard)
- **100%** = current volumeShiftedHi (shift-adjusted ceiling)

### SSE Fields Sent

| Field | Formula | Meaning |
|-------|---------|---------|
| `userVolume` | `getVolumeWebMultiplier()` | Current multiplier (1.0 = neutral) |
| `audioLo` | `Globals::audioLo` | Left boundary (silence guard) |
| `audioHi` | `getVolumeShiftedHi()` | Right boundary (shift ceiling) |
| `audioMax` | `MAX_AUDIO_VOLUME` | Hardware max (for reference) |

### JS Calculation

```javascript
loPct = Math.round((audioLo / audioMax) * 100)   // e.g. (0.05 / 0.47) * 100 = 11%
hiPct = Math.round((audioHi / audioMax) * 100)   // e.g. (0.50 / 0.47) * 100 = 106% → PROBLEM!
```

## THE BUG

When `audioHi > audioMax` (shift ceiling > hardware max), the calculation yields `hiPct > 100%`.

**Why it happens:**
- `getVolumeShiftedHi()` returns 0.50 (from shift: 1.02 × 0.5 maxAudioVolume, clamped at 0.5)
- `MAX_AUDIO_VOLUME` is 0.47
- So audioHi (0.50) > audioMax (0.47)

**Root cause:** Two different "max" values:
1. `Globals::maxAudioVolume` (0.5) - used for shift clamping
2. `MAX_AUDIO_VOLUME` (0.47) - used for SSE audioMax

These MUST be the same, or the math breaks.

## Correct Architecture

Option A: Use `MAX_AUDIO_VOLUME` everywhere
- Change shift clamping to use `MAX_AUDIO_VOLUME`
- audioHi can never exceed audioMax

Option B: Use `Globals::maxAudioVolume` for SSE audioMax
- SSE sends the same value used for clamping
- Consistent, but allows exceeding hardware max

Option C: Clamp volumeShiftedHi to `MAX_AUDIO_VOLUME` after shift
- Keeps both values but enforces hardware limit

## Recommendation

**Option A** - Single source of truth. Remove or align `Globals::maxAudioVolume` with `MAX_AUDIO_VOLUME`. The hardware limit should be THE limit, not a separate configurable value.
