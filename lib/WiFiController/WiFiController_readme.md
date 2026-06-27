# WiFiController — WiFi Networking & External Data

**Role in Kwal27:** Manages WiFi station connection with AP fallback, fetches external data (weather, sunrise/sunset via Open-Meteo API, NTP time), and pushes CSV backups to a NAS.

## Files

### WiFiController.h / WiFiController.cpp
WiFi connection manager. `bootWiFiConnect()` begins a non-blocking WiFi station connection attempt using credentials from `sdroot/wifi.txt`. On failure, falls back to AP mode (configurable SSID/password). Manages WiFi events (connected, disconnected, got IP) and maintains connection state.

### WiFiManager.cpp (no .h)
Lightweight manager stub that includes `WiFiController.h`. Provides a stable interface point for the rest of the firmware to reference the WiFi subsystem. Currently 8 lines.

### FetchController.h / FetchController.cpp
HTTP data fetcher. `bootFetchController()` initializes the HTTP client and schedules periodic fetches. API:
- `requestNtpResync()` — force NTP time re-synchronization (called at midnight via timer)
- Periodic weather fetch: pulls current conditions from Open-Meteo API, stores in ContextController
- Periodic sunrise/sunset fetch: pulls daily sun times for calendar-driven behavior

### FetchManager.cpp (no .h)
Lightweight manager stub that includes `FetchController.h`. Currently 8 lines.

### NasBackup.h / NasBackup.cpp
NAS backup push system. After CSV files are saved (pattern/color edits via web UI), `requestPush(filename)` marks the file as pending upload. A repeating timer checks readiness (WiFi connected, NAS reachable via health check, SD not busy, audio not playing) and pushes the file via HTTP POST to the NAS. Also provides periodic NAS health probing via `checkHealth()` and `startHealthTimer()`.