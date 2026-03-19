# Audio Volume Semantics

> Version: 260319A | Updated: 2026-03-19

This document defines the exact meaning of every audio volume value in the system.

## The Volume Chain

Final output volume = `getVolumeShiftedHi() * getVolumeWebMultiplier()`

Where:
- `getVolumeShiftedHi()` = system-controlled ceiling (shift-adjusted)
- `getVolumeWebMultiplier()` = user's web slider multiplier (0.0+)

The result is passed to `audioOutput.SetGain(...)`.

## Value Definitions

### Hardware Constant

| Name | Location | Value | Meaning |
|------|----------|-------|---------|
| `MAX_VOLUME` | HWconfig.h | 0.47f | Absolute hardware maximum. Speaker/amp limit. NEVER exceeded. |

### Globals Constants (CSV-overridable)

| Name | Location | Default | Meaning |
|------|----------|---------|---------|
| `Globals::volumeLo` | Globals.h | 0.05f | Silence guard. Minimum playback level. |
| `Globals::volumeHi` | Globals.h | MAX_VOLUME | Upper boundary before shifts. |
| `Globals::basePlaybackVolume` | Globals.h | 0.6f | Default SetGain for fragment playback. |
| `Globals::defaultAudioSliderPct` | Globals.h | 70 | Default slider position (0-100). |
| `Globals::loPct` | Globals.h | 0 | Slider left boundary (constexpr). |
| `Globals::hiPct` | Globals.h | 100 | Slider right boundary (constexpr). |

### State Variables (AudioState.cpp)

| Variable | Getter/Setter | Default | Meaning |
|----------|---------------|---------|---------|
| `g_volumeShiftedHi` | `getVolumeShiftedHi()` / `setVolumeShiftedHi()` | 0.37f | Current ceiling after shift application. Updated by `AudioRun::applyVolumeShift()`. |
| `g_volumeWebMultiplier` | `getVolumeWebMultiplier()` / `setVolumeWebMultiplier()` | 1.0f | User's multiplier from web slider. 1.0 = neutral. |

### Computed Value

| Function | Formula | Meaning |
|----------|---------|---------|
| `getAudioSliderPct()` | Computed from current state | Slider position as 0-100 percentage. |

## Shift Application Flow

```
AudioShiftTable::getVolumeMultiplier(statusBits)
    -> returns 0.0-1.0+ (can exceed 1.0 for boost)
    
AudioRun::applyVolumeShift(statusBits)
    -> shiftedHi = volumeMultiplier * MAX_VOLUME
    -> clamp to [0, MAX_VOLUME]
    -> setVolumeShiftedHi(shiftedHi)
```

MAX_VOLUME is the single source of truth for clamping. The shift can never
produce a value that exceeds the hardware maximum.

## WebGUI Slider Semantics

The audio slider shows a percentage where:
- **0%** = volumeLo (silence guard, 0.05)
- **100%** = current volumeShiftedHi (shift-adjusted ceiling)

### SSE Fields Sent

| SSE Field | Source | Meaning |
|-----------|--------|---------|
| `audioSliderPct` | `getAudioSliderPct()` | Current slider position (0-100) |
| `volumeLo` | `Globals::volumeLo` | Left boundary (silence guard) |
| `volumeHi` | `getVolumeShiftedHi()` | Right boundary (shift ceiling) |
| `volumeMax` | `MAX_VOLUME` | Hardware max (0.47) |

### SSE Push Events

| Event | Trigger |
|-------|---------|
| `state` | On connect + after any setter |

## Other Audio State

AudioState.cpp also tracks playback state:

| Function | Meaning |
|----------|---------|
| `isFragmentPlaying()` / `setFragmentPlaying()` | Fragment currently playing |
| `isSentencePlaying()` / `setSentencePlaying()` | TTS sentence playing |
| `isTtsActive()` / `setTtsActive()` | TTS engine active |
| `isAudioBusy()` / `setAudioBusy()` | Any audio output active |
| `getAudioLevelRaw()` / `setAudioLevelRaw()` | Raw audio level (int16_t) |
| `getCurrentDirFile()` / `setCurrentDirFile()` | Current dir/file/score triplet |
