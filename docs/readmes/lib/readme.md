# Kwal27 - An Art Project (Jellyfish)

> Version: 260313H | Updated: 2026-03-14

## Project Overview

ESP32-S3 ambient art installation (jellyfish sculpture) with LED lights, audio playback, web interface, and sensor integration. PlatformIO project, Arduino framework, 16MB flash.

**Core subsystems:**
- **Audio** — MP3 fragment playback (random 6-48 min intervals), TTS via PlaySentence, PCM clips, I2S output
- **LED** — 160 WS2812B LEDs with pattern-based light shows, position-mapped (x,y from ledmap.bin), TV simulator mode
- **Web Interface** — Browser UI (volume, brightness, pattern/color selection, OTA, diagnostics, lux calibration)
- **WiFi & OTA** — Auto-connect with retry, static IP, wireless firmware update
- **Sensors** — VEML7700 ambient lux, VL53L1X time-of-flight distance
- **Time** — DS3231 RTC + NTP sync, sunrise/sunset calculation
- **SD Card** — Index-driven MP3 selection, CSV configs, NAS fetch with SD fallback

## Architecture

```
TimerManager (central timing) → RunManager (orchestration)
    ↓                              ↓
Boot layers (one-time init)    Policy layers (rules/constraints)
    ↓                              ↓
Controllers (hardware APIs)    Directors (context→requests)
```

**Flow:** producers → Context → Run → Policies → Requests → Controllers (audio/light/serial)

| Layer | Role | Constraints |
|-------|------|-------------|
| **Boot** | One-time init, register timers, seed caches. BootSequencer manages dependencies via Cap grants. | Runs once at startup |
| **Run** (`lib/RunManager/`) | Orchestration `cb_*` callbacks, sequences work, raises requests | 14 domain subdirectories |
| **Policy** | Domain rules, approve/deny requests | NO side effects, NO timers, NO hardware |
| **Director** | Build requests from context | NO policy decisions |
| **Controller** | Hardware drivers (FastLED, I2S, SPI) | ONLY controllers touch hardware |

**Status ownership:** Controllers write to AlertState; all reads from AlertState, not controller APIs.

## Library Modules

| Module | Purpose |
|--------|---------|
| `AudioManager/` | MP3 playback coordinator: PlayFragment (fade), PlaySentence (TTS), PlayPCM |
| `ClockController/` | DS3231 RTC driver, NTP sync |
| `ContextController/` | Runtime state aggregation (calendar, statusflags, time-of-day, theme boxes) |
| `Globals/` | Shared config, MathUtils, CsvUtils, AudioState, macros, HWconfig |
| `LightController/` | LED show renderer, TvShow (6-ring TV simulator), LED map |
| `RunManager/` | 14 subdirs: Alert, Audio, Boot, Calendar, Clock, Heartbeat, Light, SD, Sensors, Speak, Status, System, Web, WiFi |
| `SDController/` | Index-driven MP3 selection (binary .root_dirs/.files_dir), voting |
| `SdFileAccess/` | SD card file I/O, path utilities, busy guard |
| `SensorController/` | VEML7700 (lux), VL53L1X (distance); uniform driver API |
| `TimerManager/` | 60-slot non-blocking timer system: create/restart/cancel, growth backoff |
| `WebInterfaceController/` | REST API (10 route files), SSE, fallback page |
| `WiFiController/` | WiFi connect/retry/health, NAS CSV fetch |

## Key Design Rules

- **TimerManager only** — no `millis()`, `delay()`, `esp_timer`, or `Ticker`
- **Web handlers: memory only** — no SD I/O or network I/O from handlers; defer to timer callbacks
- **Subsystem isolation** — subsystems communicate through shared state or RunManager, never direct calls
- Each module has one global instance; dependencies flow top-down: context → run → policy → modules
