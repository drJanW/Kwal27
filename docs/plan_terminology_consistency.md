# Plan: Brightness/Volume Terminology Consistency

> Version: 260101A | Created: 2026-01-01

## Goal

Establish consistent naming patterns between brightness and volume systems.

## Target Pattern

| Concept | Brightness | Volume |
|---------|------------|--------|
| Hardware Max | `MAX_BRIGHTNESS` | `MAX_VOLUME` |
| Lo boundary (Globals) | `Globals::brightnessLo` | `Globals::volumeLo` |
| Hi unshifted (Globals) | `Globals::brightnessUnshiftedHi` | `Globals::volumeUnshiftedHi` |
| Hi shifted (runtime) | `getBrightnessShiftedHi()` | `getVolumeShiftedHi()` |
| Web modifier (runtime) | `getBrightnessWebModifier()` | `getVolumeWebModifier()` |
| SSE Lo | `brightnessLo` | `volumeLo` |
| SSE Hi | `brightnessHi` | `volumeHi` |
| SSE Max | `brightnessMax` | `volumeMax` |
| SSE user value | `userBrightness` | `userVolume` |

## Current → Target Renames

### Hardware Constants (HWconfig.h)
| Current | Target |
|---------|--------|
| `MAX_AUDIO_VOLUME` | `MAX_VOLUME` |
| `MAX_BRIGHTNESS` | (keep) |

### Globals (Globals.h)
| Current | Target |
|---------|--------|
| `luxMinBase` | `brightnessLo` |
| `audioLo` | `volumeLo` |
| `baseGain` | `volumeUnshiftedHi` |
| `maxAudioVolume` | **REMOVE** (use MAX_VOLUME) |
| (none) | `brightnessUnshiftedHi` (if needed) |

### AudioState (AudioState.h/cpp)
| Current | Target |
|---------|--------|
| `g_baseGain` | `g_volumeShiftedHi` |
| `getBaseGain()` | `getVolumeShiftedHi()` |
| `setBaseGain()` | `setVolumeShiftedHi()` |
| `g_webAudioLevel` | `g_volumeWebModifier` |
| `getWebAudioLevel()` | `getVolumeWebModifier()` |
| `setWebAudioLevel()` | `setVolumeWebModifier()` |

### LightState (if exists, or LightManager)
| Current | Target |
|---------|--------|
| `getWebBrightness()` ? | `getBrightnessWebModifier()` |
| `setWebBrightness()` ? | `setBrightnessWebModifier()` |

### SSE Fields (WebGuiStatus.cpp)
| Current | Target |
|---------|--------|
| `audioLo` | `volumeLo` |
| `audioHi` | `volumeHi` |
| `audioMax` | `volumeMax` |
| `brightnessLo` | (keep) |
| `brightnessHi` | (keep) |
| `brightnessMax` | (keep) |

### JS (main.js)
| Current | Target |
|---------|--------|
| `data.audioLo` | `data.volumeLo` |
| `data.audioHi` | `data.volumeHi` |
| `data.audioMax` | `data.volumeMax` |

## Execution Order

1. **Phase 1: Hardware constants** (HWconfig.h)
   - Rename `MAX_AUDIO_VOLUME` → `MAX_VOLUME`
   - Update all usages

2. **Phase 2: Globals** (Globals.h, Globals.cpp, globals.csv)
   - Rename `luxMinBase` → `brightnessLo`
   - Rename `audioLo` → `volumeLo`
   - Rename `baseGain` → `volumeUnshiftedHi`
   - Remove `maxAudioVolume`
   - Update all usages

3. **Phase 3: Audio state** (AudioState.h/cpp)
   - Rename getters/setters per table above
   - Update all usages

4. **Phase 4: SSE** (WebGuiStatus.cpp)
   - Rename JSON field names
   - Coordinate with JS changes

5. **Phase 5: JS** (main.js, audio.js)
   - Update SSE field references
   - Build + upload

6. **Phase 6: Documentation**
   - Update AUDIO_GAIN_SEMANTICS.md → VOLUME_SEMANTICS.md
   - Update glossary_slider_semantics.md
   - Update any READMEs

## Rules Reminder

- Use "gain" ONLY for I2S hardware (`SetGain()`)
- Use "volume" for everything else
- Pattern: `{domain}{Adjective}{Boundary}` e.g. `volumeShiftedHi`
- Lo/Hi = operational boundaries (can move)
- Min/Max = hardware limits (fixed)

## Risk

Large refactor touching many files. Should be done incrementally with compile+test between phases.

## Status

- [ ] Phase 1: Hardware constants
- [ ] Phase 2: Globals
- [ ] Phase 3: Audio state
- [ ] Phase 4: SSE
- [ ] Phase 5: JS
- [ ] Phase 6: Documentation
