# Web Interface Controller

> Version: 260205D | Updated: 2026-02-05

## Purpose
Serve a lightweight UI from the SD card so operators can monitor and steer the installation without reflashing firmware. The ESP32 hosts `index.html`, `styles.css`, and `kwal.js` directly from the SD root; updates only require copying new files and bumping the cache-buster query (`?v=`) in `index.html`.

## Current Features
1. **Audio panel** – Displays the current fragment, exposes a slider for web volume (`/setWebAudioLevel`, `/getWebAudioLevel`), and shows status text from `AudioManager`.
2. **Light panel** – Presents brightness control (`/setBrightness`, `/getBrightness`) and summary values from `LightController`.
3. **OTA modal** – Single **Start OTA** button posts to `/ota/start`, displays a 15 s countdown, and reminds the operator to press Enter in `ota.bat` once the device reboots into ArduinoOTA mode.
4. **SD controller modal** – File listing and upload/delete helpers talking to `SDController` REST endpoints (`/api/sd/*`).
5. **Diagnostics** – `kwal.js` exposes `window.APP_BUILD_INFO` so the UI can verify whether the browser has the latest assets.

## REST Endpoints
- `GET /` → streams `index.html` from SD (returns 503 if SD not ready).
- `GET /setBrightness?value=X` and `GET /getBrightness` – route to `RunManager` requests for light control.
- `GET /setWebAudioLevel` and `GET /getWebAudioLevel` – adjust web-facing audio gain.
- `POST /ota/start` – arms + confirms OTA window (replaces the old `/ota/arm` + `/ota/confirm` combo).
- `GET /api/sd/status`, `POST /api/sd/upload`, `POST /api/sd/delete` – SD maintenance.

Routes live in `WebInterfaceController.cpp`; each maps directly to a `RunManager` request or SDController method. Logging uses `PF` macros guarded by `WEBIF_LOG_LEVEL`.

## SSE Reconnect Behavior

The WebGUI uses Server-Sent Events (SSE) for real-time updates. When the connection drops and reconnects (`sse.js` `onReconnect` callback):

- Pattern and color selections are automatically refreshed from the device
- UI state is synchronized without requiring manual page reload
- Prevents stale UI after network interruptions or device restarts

Note: Pattern next/prev no longer forces an immediate status refresh; this avoids duplicate `/api/patterns` fetches that can cause transient UI "?" under load.

## Asset Refresh Workflow
1. Edit the files under `sdroot/`.
2. Update the version string in `kwal.js` (`APP_BUILD_INFO.version`) and the `?v=` query parameters in `index.html`.
3. Copy the files to the SD card and reboot the board. The UI self-validates via the version tag.

## Future Work
- Push status polling (heartbeat, Wi-Fi info) into the UI header.
- Add error surfaces for failing uploads (currently shown as toast text).
- Optional translation hooks for non-Dutch operators.
