# Audio Gain Semantics

> Version: 260101A | Updated: 2026-01-01

This document defines the exact meaning of every audio volume/gain value in the system.

## The Gain Chain

Final output gain = `baseGain × webAudioLevel × fadeFactor`

Where:
- `baseGain` = system-controlled ceiling (shift-adjusted)
- `webAudioLevel` = user's web slider modifier (0.0-1.0)
- `fadeFactor` = transient fade envelope (0.0-1.0)

## Value Definitions

### Hardware Constants

| Name | Location | Value | Meaning |
|------|----------|-------|---------|
| `MAX_AUDIO_VOLUME` | HWconfig.h | 0.47f | Absolute hardware maximum. Speaker/amp limit. NEVER exceeded. |

### Runtime Values

| Name | Location | Default | Meaning |
|------|----------|---------|---------|
| `Globals::maxAudioVolume` | Globals.h / globals.csv | 0.5f (code) or 0.37 (csv) | Master ceiling from config. Used by shift calculation. |
| `Globals::baseGain` | Globals.h / globals.csv | 0.37f | Initial base gain before any shifts. |
| `Globals::audioLo` | Globals.h | 0.05f | Silence guard. Minimum playback level to avoid near-zero. |

### State Variables (AudioState.cpp)

| Name | Getter/Setter | Meaning |
|------|---------------|---------|
| `g_baseGain` | `getBaseGain()` / `setBaseGain()` | Current ceiling after shift application. Initialized from `Globals::baseGain`, updated by `AudioRun::applyVolumeShift()`. |
| `g_webAudioLevel` | `getWebAudioLevel()` / `setWebAudioLevel()` | User's modifier from web slider. 0.0 = muted, 1.0 = full (up to baseGain). |

## Shift Application Flow

```
AudioShiftStore::getEffectiveVolume(statusBits)
    → returns 0.0-1.0+ (can exceed 1.0 for boost)
    
AudioRun::applyVolumeShift(statusBits)
    → scaledVolume = effectiveVolume × Globals::maxAudioVolume
    → clamp to [0, Globals::maxAudioVolume]
    → setBaseGain(scaledVolume)
```

Log: `[AudioRun] Volume shift: 0.50 (eff=1.02, status=0x...)`
- `0.50` = scaledVolume stored in baseGain
- `1.02` = raw effectiveVolume from shift (>1.0 means boost)

## WebGUI Slider Semantics

The slider shows a percentage where:
- **0%** = audioLo (silence guard)
- **100%** = current baseGain (shift-adjusted ceiling)

### SSE Fields Sent

| Field | Formula | Meaning |
|-------|---------|---------|
| `userVolume` | `getWebAudioLevel()` | Current modifier 0.0-1.0 |
| `audioLo` | `Globals::audioLo` | Left boundary (silence guard) |
| `audioHi` | `getBaseGain()` | Right boundary (shift ceiling) |
| `audioMax` | `MAX_AUDIO_VOLUME` | Hardware max (for reference) |

### JS Calculation

```javascript
loPct = Math.round((audioLo / audioMax) * 100)   // e.g. (0.05 / 0.47) * 100 = 11%
hiPct = Math.round((audioHi / audioMax) * 100)   // e.g. (0.50 / 0.47) * 100 = 106% → PROBLEM!
```

## THE BUG

When `audioHi > audioMax` (shift ceiling > hardware max), the calculation yields `hiPct > 100%`.

**Why it happens:**
- `getBaseGain()` returns 0.50 (from shift: 1.02 × 0.5 maxAudioVolume, clamped at 0.5)
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

Option C: Clamp baseGain to `MAX_AUDIO_VOLUME` after shift
- Keeps both values but enforces hardware limit

## Recommendation

**Option A** - Single source of truth. Remove or align `Globals::maxAudioVolume` with `MAX_AUDIO_VOLUME`. The hardware limit should be THE limit, not a separate configurable value.
