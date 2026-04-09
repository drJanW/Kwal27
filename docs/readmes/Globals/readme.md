# Globals - Shared Configuration

> Version: 260409G | Updated: 2026-04-09

Contains all parameters used by more than one module or file.

## Files

| File | Purpose |
|------|---------|
| `Globals.h` | Shared config defines, `FIRMWARE_VERSION_CODE`, extern declarations |
| `Globals.cpp` | Atomic state accessors (get/set pattern) |
| `HWconfig.h` | Hardware pins, Wi-Fi credentials, static IP, `*_PRESENT` flags |
| `macros.inc` | Logging macros (`PL`/`PP`/`PF`), module verbosity toggles, `SECONDS`/`MINUTES`/`HOURS` |
| `MathUtils.h` | `clamp`, `map`, `mapRange`, `lerp`, `inverseLerp`, `wrap`, `nearlyEqual`, etc. |
| `CsvUtils.h` / `.cpp` | CSV parsing: `csv::readLine()`, `csv::splitColumns()`, `csv::stripBom()`, `csv::isNumericId()` |
| `AudioState.h` / `.cpp` | Thread-safe audio state accessors (moved from AudioManager for cross-layer access) |
| `LogBuffer.h` / `.cpp` | Ring buffer for in-memory log capture |
| `I2CInitHelper.h` / `.cpp` | Shared I²C bus initialization |

## Atomics helper pattern

`Globals.cpp` exposes helper templates for light-weight atomic storage. Typical usage:

```cpp
template <typename T>
inline void setMux(T value, std::atomic<T>* ptr) {
    ptr->store(value, std::memory_order_relaxed);
}
template <typename T>
inline T getMux(const std::atomic<T>* ptr) {
    return ptr->load(std::memory_order_relaxed);
}

static std::atomic<uint8_t> _valYear{0};
void setYear(uint8_t v) { setMux(v, &_valYear); }
uint8_t getYear() { return getMux(&_valYear); }
```

Most global state accessors follow this pattern and should migrate into dedicated controllers over time.

`HWconfig.h` keeps hardware settings (pins, Wi-Fi credentials, static IP, etc.). `adc_port` is still only used to seed randomness and should be refactored out later.

## CsvUtils (`CsvUtils.h`, v260302C)

Namespace `csv::` provides safe CSV parsing utilities used by all CSV loaders:

- `readLine(file, out)` — reads one line, strips CR/LF
- `splitColumns(line, vector, delimiter=';')` — parses semicolon-delimited columns
- `stripBom(text)` — removes UTF-8 BOM from first line
- `isNumericId(id)` — validates numeric ID strings (skips comment lines starting with `#`)

## AudioState (`AudioState.h`, v260227B)

Thread-safe audio state accessors moved from `AudioManager` to `Globals` for cross-layer access. Uses relaxed atomic ordering. Key accessors:

- `isFragmentPlaying()` / `isSentencePlaying()` / `isTtsActive()` / `isPcmPlaying()`
- `getVolumeWebMultiplier()` / `setVolumeWebMultiplier()`
- Track info, audio meter level, playback status flags

## Runtime config override (`globals.csv`)

Most parameters can be overridden at boot via `globals.csv` on SD. Lines prefixed with `#` use code defaults. Format: `name;type;value;comment` where type is `u` (uint32), `f` (float), or `s` (string).

## Audio timing constants

`Globals::minAudioIntervalMs` and `Globals::maxAudioIntervalMs` define the random range for audio fragment intervals (default 6-48 minutes). Audio playback picks a random delay within this window after each fragment completes.

## Error Flash Notification Constants (v260104+)

Configurable parameters for hardware error flash notifications in `Globals.h`:

| Parameter | Default | Description |
|-----------|---------|-------------|
| `flashBurstIntervalMs` | 10s | Interval between flash bursts |
| `flashBurstRepeats` | 1 | Repeats after initial (1 = 2 bursts total) |
| `flashBurstGrowth` | 1.0 | Multiplier for burst interval growth |
| `reminderIntervalMs` | 2min | First reminder delay after boot burst |
| `reminderIntervalGrowth` | 10.0 | Multiplier for growing reminder intervals |

One flash burst = `black(1s) + color(1-2s) + black(1s)` ≈ 3-4s per failing component.

## Daily reboot

| Parameter | Default | Description |
|-----------|---------|-------------|
| `dailyRebootHour` | 4 | Hour (1-23) for daily auto-reboot. 0 = disabled (midnight not possible since 0 means off). |

## TTS Cache Parameters (v260409G)

| Parameter | Type | Default | Range | Description |
|-----------|------|---------|-------|-------------|
| `ttsCacheDirIndex` | u | 127 | 0-200 | SD directory index for cached TTS MP3 |
| `ttsCacheFileIndex` | u | 0 | 0-101 | File index within cache directory (000.mp3) |
| `ttsCacheDurationFactor` | f | 3.0 | 1.0-5.0 | Multiplier for filesize→duration estimation |

Duration estimation: `(bytesWritten * ttsCacheDurationFactor) / 16` milliseconds. Factor 3.0 compensates for VoiceRSS outputting ~48kbps (vs the 128kbps assumed by the standard `/16` formula).

## Diagnostics flags

`SHOW_TIMER_STATUS` (default 0): set to 1 to enable periodic timer slot diagnostics in the serial log. Useful for debugging timer exhaustion or slot leaks.

## Logging controls (`macros.inc`)

`macros.inc` centralizes logging macros for every module:

- Default build uses `LOG_LEVEL_NONE`. Override per build via PlatformIO flag, e.g. `-DLOG_LEVEL=LOG_LEVEL_INFO` for informational output or `LOG_LEVEL_DEBUG` for full tracing.
- Heartbeat dots are guarded by `LOG_HEARTBEAT`; enable with `-DLOG_HEARTBEAT=1` when you want a periodic serial heartbeat, otherwise leave at `0` to avoid clutter.
- Module-level verbosity toggles such as `LOG_RUN_VERBOSE`, `LOG_TIMER_VERBOSE`, `LOG_AUDIO_VERBOSE`, etc. gate the chattier traces. Define them as `1` (either in `platformio.ini` or a local header) when diagnosing that subsystem.
- Use `PL()/PP()/PF()` helpers for INFO-level lines; `LOG_ERROR/LOG_WARN/LOG_INFO/LOG_DEBUG` remain available for formatted output.

When the serial monitor shows nothing except ESP-IDF boot ROM text, confirm the firmware was built with at least `LOG_LEVEL_INFO` so the `[Main] Version …` banner is visible.
