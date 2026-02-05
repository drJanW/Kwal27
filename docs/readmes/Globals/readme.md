# Globals - Shared Configuration

> Version: 260205D | Updated: 2026-02-05

Contains all parameters used by more than one module or file.

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

## Diagnostics flags

`SHOW_TIMER_STATUS` (default 0): set to 1 to enable periodic timer slot diagnostics in the serial log. Useful for debugging timer exhaustion or slot leaks.

## Logging controls (`macros.inc`)

`macros.inc` centralizes logging macros for every module:

- Default build uses `LOG_LEVEL_NONE`. Override per build via PlatformIO flag, e.g. `-DLOG_LEVEL=LOG_LEVEL_INFO` for informational output or `LOG_LEVEL_DEBUG` for full tracing.
- Heartbeat dots are guarded by `LOG_HEARTBEAT`; enable with `-DLOG_HEARTBEAT=1` when you want a periodic serial heartbeat, otherwise leave at `0` to avoid clutter.
- Module-level verbosity toggles such as `LOG_RUN_VERBOSE`, `LOG_TIMER_VERBOSE`, `LOG_AUDIO_VERBOSE`, etc. gate the chattier traces. Define them as `1` (either in `platformio.ini` or a local header) when diagnosing that subsystem.
- Use `PL()/PP()/PF()` helpers for INFO-level lines; `LOG_ERROR/LOG_WARN/LOG_INFO/LOG_DEBUG` remain available for formatted output.

When the serial monitor shows nothing except ESP-IDF boot ROM text, confirm the firmware was built with at least `LOG_LEVEL_INFO` so the `[Main] Version …` banner is visible.
