# WebInterfaceController — Web Server & REST API

**Role in Kwal27:** Serves the web GUI, handles REST API endpoints for all subsystems, and pushes real-time status to the browser via Server-Sent Events (SSE). Built on ESPAsyncWebServer for non-blocking operation.

## Top-Level Files

### WebInterfaceController.h / WebInterfaceController.cpp
Entry point for the web subsystem. `beginWebInterface()` initializes the async web server, mounts all route handlers from `routes/`, configures SPIFFS or SD as the web root, and starts the SSE event source. `updateWebInterface()` is a periodic tick for SSE push scheduling.

### WebInterfaceManager.cpp (no .h)
Lightweight manager stub that includes `WebInterfaceController.h`. Provides a stable interface point for the rest of the firmware to reference the web subsystem. Currently 8 lines.

### WebGuiStatus.h / WebGuiStatus.cpp
Centralized state management for the web GUI's Server-Sent Events stream. Maintains an atomic struct of live values (brightness, audio level, current fragment dir/file/score) and pushes updates to connected browsers on change. API:
- `setBrightness(value)` / `setAudioLevel(value)` — push brightness/audio to browser
- `setFragment(dir, file, score, durationMs)` — push currently playing fragment info
- `setFragmentScore(score)` — update only the score (vote changes)
- `getFragmentDir()` / `getFragmentFile()` / `getFragmentScore()` — read current state
- SSE push functions: `pushState()`, `pushEvent()`, `pushPing()`

### FallbackPage.h / FallbackPage.cpp
Minimal HTML page (~2.5 KB) stored in PROGMEM (flash). Served when the SD card is unavailable, providing basic functionality: status overview at `/api/health`, OTA firmware upload, and restart button. The full web GUI (index.html, kwal.js, styles.css) is served from SD when available.

### WebUtils.h
Shared utilities for route handlers (header-only):
- `sendJson(request, payload)` — send JSON response with no-cache headers
- `sendError(request, code, message)` — send error response
- `appendJsonEscaped(str, value)` — JSON-escape a string
- `rgbToHex(r, g, b)` — convert RGB to hex string for JSON

### AsyncJsonCompat.cpp
Compatibility layer bridging ESPAsyncWebServer JSON handling across library versions.

## routes/ — REST API Route Handlers

Each `*Routes.h/*.cpp` pair registers REST endpoints for one subsystem:

| File Pair | Routes | Purpose |
|-----------|--------|---------|
| `AdminRoutes` | `/api/admin/*` | Firmware status, restart, heap info |
| `AudioRoutes` | `/api/audio/*` | Audio playback control, current track info |
| `ColorsRoutes` | `/api/colors/*` | Light color catalog CRUD |
| `HealthRoutes` | `/api/health` | System health endpoint (JSON status) |
| `LogRoutes` | `/api/log/*` | Log buffer retrieval |
| `LuxCalRoutes` | `/api/luxcal/*` | Ambient light calibration data |
| `OtaRoutes` | `/api/ota/*` | Over-the-air firmware update |
| `PatternsRoutes` | `/api/patterns/*` | Light pattern catalog CRUD |
| `SdRoutes` | `/api/sd/*` | SD card file browser, upload, delete |
| `SseController` | `/events` | Server-Sent Events stream |
| `TodayRoutes` | `/api/today/*` | Calendar, sunrise/sunset, current settings |

Each route handler validates input, performs the operation, and returns JSON. Admin and OTA routes are authentication-gated (basic auth with credentials from wifi.txt).