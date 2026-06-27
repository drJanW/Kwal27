# Kwal27 Firmware — Library Reference

Auto-generated architecture overview. Each library has a `*_readme.md` in its source directory.

## Libraries

| Library | Readme | Role |
|---------|--------|------|
| **Globals** | [Globals_readme.md](../../lib/Globals/Globals_readme.md) | Global macros, forward declarations, extern instances |
| **TimerManager** | [TimerManager_readme.md](../../lib/TimerManager/TimerManager_readme.md) | Non-blocking timer pool (all scheduling) |
| **LightController** | [LightController_readme.md](../../lib/LightController/LightController_readme.md) | LED light show engine (CircleShow + RingShow) |
| **AudioManager** | [AudioManager_readme.md](../../lib/AudioManager/AudioManager_readme.md) | MP3 playback via VS1053 codec |
| **ClockController** | [ClockController_readme.md](../../lib/ClockController/ClockController_readme.md) | RTC timekeeping + NTP sync |
| **ContextController** | [ContextController_readme.md](../../lib/ContextController/ContextController_readme.md) | Shared application state bus |
| **SensorController** | [SensorController_readme.md](../../lib/SensorController/SensorController_readme.md) | Hardware sensors (lux, distance, IMU) |
| **WiFiController** | [WiFiController_readme.md](../../lib/WiFiController/WiFiController_readme.md) | WiFi networking + external data fetch + NAS backup |
| **WebInterfaceController** | [WebInterfaceController_readme.md](../../lib/WebInterfaceController/WebInterfaceController_readme.md) | Web server + REST API + SSE push |
| **SDController** | [SDController_readme.md](../../lib/SDController/SDController_readme.md) | SD card index + file manager + voting |
| **SdFileAccess** | [SdFileAccess_readme.md](../../lib/SdFileAccess/SdFileAccess_readme.md) | Thread-safe SD I/O abstraction layer |
| **RunManager** | [RunManager_readme.md](../../lib/RunManager/RunManager_readme.md) | Orchestration engine + 15 run policy directors |

## SD Card Data

| Resource | Path | Purpose |
|----------|------|---------|
| Web GUI | `sdroot/index.html` + `kwal.js` + `styles.css` | Single-page app served from SD |
| Web GUI source | `sdroot/webgui-src/` | Unminified source; build with `build_js.ps1` |
| CSV data files | `sdroot/*.csv` | Patterns, colors, calendar, globals, theme boxes, shifts |

## Source Entry Point

| File | Purpose |
|------|---------|
| `src/main.cpp` | `setup()` + `loop()` — initializes all subsystems, runs TimerManager + RunManager each tick |

## Tools & Scripts

| Path | Purpose |
|------|---------|
| `tools/generate_ledmap.py` | Generate LED polar coordinate map |
| `tools/verify_ledmap.py` | Verify LED map against ring layout |
| `tools/update_headers.ps1` | Bulk-update `@version` headers |
| `deploy.ps1` | Build + upload to ESP32 |
| `upload_csv.ps1` | Upload CSV data files to device |
| `upload_web.ps1` | Upload web GUI files to device |
| `upload_file.ps1` | Upload single file to device |
| `build_js.ps1` | Minify webgui-src → sdroot |
| `zip.ps1` | Create release zip archive |
| `connect_nas.ps1` | Mount NAS network drive |

## External Documentation

| Path | Topic |
|------|-------|
| `docs/plan_ringshow.md` | RingShow rendering design |
| `docs/manual.txt` | User manual |
| `docs/sd_card_files.md` | SD card file layout specification |
| `docs/readmes/LightController/readme.md` | LightController deep-dive |
| `docs/readmes/sdroot/readme.md` | SD root web files reference |