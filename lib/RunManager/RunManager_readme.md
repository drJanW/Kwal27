# RunManager — Orchestration & Policy Engine

**Role in Kwal27:** The application-orchestration layer. `RunManager` is the top-level state machine that coordinates all subsystem activity — boot sequencing, background maintenance (WiFi, sensors, SD), and the 15 "Run" policy directors that determine what the device actually does (play audio, render light shows, speak, etc.). Runs on top of `TimerManager` for periodic ticks and `ContextController` for state.

## Top-Level Files

### RunManager.h / RunManager.cpp
Master orchestrator. `RunManager::update()` is called once per main loop and dispatches to:
- **Boot phase** — `BootManager` initializes hardware in a defined sequence
- **Background phase** — `RunManager::updateBackground()` periodically ticks WiFi, sensors, SD checks, and log buffer
- **Run phase** — active Run subclass (`_run->update()`) receives the full 30 fps tick

`RunManager` creates and owns all run instances (`_boot`, `_run`, `_web`, `_idle`, `_alert`, `_busy`). Transitions between runs via `setRun()`.

### BootManager.h / BootManager.cpp
Boot sequencer running inside RunManager. Initializes subsystems in order: I2C bus, SD card, RTC, WiFi, sensors, CSV catalogs (globals, colors, patterns, theme boxes), calendar, hardware status register. Tracks progress as a bitmask (`hwStatus`) to allow fast recovery on warm boot.

## Submodule Directories — "Run" Policy Directors

Each `Run<Name>/` directory contains a `Run<Name>.h` and `Run<Name>.cpp` implementing one mode of operation:

| # | Directory | Run Class | Purpose |
|---|-----------|-----------|---------|
| 1 | `Alert/` | `RunAlert` | Critical alerts — flashes LEDs, beeps speaker, takes priority over all other runs |
| 2 | `Audio/` | `RunAudio` | Main background audio playback mode: selects theme box directory, picks random fragment, plays MP3 with fade |
| 3 | `Boot/` | `RunBoot` | Boot display — shows version number, startup animation on LEDs |
| 4 | `Calendar/` | `RunCalendar` | Calendar-driven override — applies special light pattern/color for calendar dates |
| 5 | `Clock/` | `RunClock` | Time announcements — speaks the time periodically using TTS |
| 6 | `Demo/` | `RunDemo` | Demo mode — cycles through all light patterns/colors for showroom/testing |
| 7 | `Heartbeat/` | `RunHeartbeat` | Low-power idle pulse — subtle LED heartbeat when nothing else is happening |
| 8 | `Light/` | `RunLight` | Light show driver — updates light pattern/color from catalog, renders via LightController |
| 9 | `SD/` | `RunSD` | SD card maintenance — scans directories, builds file index, votes |
| 10 | `Sensors/` | `RunSensors` | Sensor polling — reads lux, distance, IMU sensors periodically |
| 11 | `Speak/` | `RunSpeak` | Speech coordinator — manages TTS queue, speaks sentences at scheduled times |
| 12 | `Status/` | `RunStatus` | Status flags — evaluates system state, sets/clears status bits |
| 13 | `System/` | `RunSystem` | System housekeeping — heap monitoring, deep sleep check, uptime tracking |
| 14 | `Tts/` | `RunTts` | TTS engine — VoiceRSS API calls, cache management |
| 15 | `Web/` | `RunWeb` | Web command handler — processes commands from web UI (next track, vote, delete, ban) |
| 16 | `WiFi/` | `RunWiFi` | WiFi management — connection state machine, AP fallback |

Each subdirectory may also contain a `README.md` with policy-specific documentation, and a `Scene*` or `*Scene` pair for complex multi-step sequences within that run.