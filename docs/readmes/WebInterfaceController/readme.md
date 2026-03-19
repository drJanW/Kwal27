# Web Interface Controller

> Version: 260319A | Updated: 2026-03-19

## Purpose
Serve a lightweight UI from the SD card so operators can monitor and steer the installation without reflashing firmware. The ESP32 hosts `index.html`, `styles.css`, and `kwal.js` directly from the SD root; updates only require copying new files and bumping the cache-buster query (`?v=`) in `index.html`.

## Current Features
1. **Audio panel** — Fragment display, web volume slider, status text
2. **Light panel** — Brightness control, pattern/color dropdowns, summary values
3. **Lux calibration panel** — Sample, fit, accept/wis cycle for VEML7700→brightness mapping
4. **TV simulator panel** — 6-ring zone color control
5. **OTA modal** — Firmware upload with progress bar and auto-reboot detection
6. **SD controller modal** — File listing, upload, delete
7. **MP3 grid** — Visual mp3 directory/file browser
8. **Log viewer** — Real-time serial log display in browser
9. **Health / diagnostics** — Component status, version, uptime
10. **Diagnostics** — `kwal.js` exposes `window.APP_BUILD_INFO` for asset version validation

## Route Architecture

Routes are organized per domain in `lib/WebInterfaceController/routes/`:

| File | Endpoints | Purpose |
|------|-----------|---------|
| `AudioRoutes` | `/setWebAudioLevel`, `/getWebAudioLevel`, `/api/audio/*` | Audio volume and fragment control |
| `ColorsRoutes` | `/api/colors`, `/api/colors/select` | Color palette CRUD |
| `PatternsRoutes` | `/api/patterns`, `/api/patterns/select` | Pattern CRUD and selection |
| `HealthRoutes` | `/api/health` | Component status, version, uptime |
| `LuxCalRoutes` | `/api/luxcal/*` | Lux calibration: sample, fit, accept, wis, status |
| `OtaRoutes` | `/ota/upload` | Firmware upload (multipart) |
| `SdRoutes` | `/api/sd/*` | SD file listing, upload, delete |
| `TodayRoutes` | `/api/today` | Calendar/theme box info |
| `LogRoutes` | `/api/log` | Serial log ring buffer |
| `SseController` | `/events` | Server-Sent Events for real-time UI updates |

Other files: `WebInterfaceController.h/.cpp` (server setup), `WebInterfaceManager.cpp` (route registration), `WebGuiStatus.h/.cpp` (status JSON builder), `WebUtils.h` (helpers), `FallbackPage.h/.cpp` (minimal page when SD unavailable), `AsyncJsonCompat.cpp` (JSON response wrapper).

## REST Endpoints
- `GET /` → streams `index.html` from SD (returns 503 if SD not ready).
- `GET /setBrightness?value=X` and `GET /getBrightness` — route to `RunManager` requests for light control.
- `GET /setWebAudioLevel` and `GET /getWebAudioLevel` — adjust web-facing audio gain.
- `POST /ota/upload` — HTTP firmware upload (multipart binary), reboots after success.
- `GET /api/sd/status`, `POST /api/sd/upload`, `POST /api/sd/delete` — SD maintenance.
- `GET /api/luxcal/status`, `POST /api/luxcal/sample`, etc. — lux calibration workflow.

All web handlers follow the **memory-only** rule: set flags/variables in memory, never perform SD I/O or network I/O directly. Actual work is deferred to timer callbacks.

## SSE Reconnect Behavior

The WebGUI uses Server-Sent Events (SSE) for real-time updates. When the connection drops and reconnects (`sse.js` `onReconnect` callback):

- Pattern and color selections are automatically refreshed from the device
- UI state is synchronized without requiring manual page reload
- Prevents stale UI after network interruptions or device restarts

Note: Pattern next/prev no longer forces an immediate status refresh; this avoids duplicate `/api/patterns` fetches that can cause transient UI "?" under load.

## Asset Refresh Workflow
1. Edit the files under `sdroot/` (JS sources: `webgui-src/js/`).
2. Run `build.ps1` to rebuild `kwal.js`; this also updates the `APP_BUILD_INFO.version` and `?v=` cache-buster in `index.html`.
3. Upload to SD card with `upload_web.ps1` and reboot.

## Future Work
- Optional translation hooks for non-Dutch operators.
