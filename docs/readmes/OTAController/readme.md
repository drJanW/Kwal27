# OTA Controller – Push Workflow

> Version: 260205D | Updated: 2026-02-05

## Overview
The OTA controller now runs a **single-button ArduinoOTA push** flow. The ESP32 exposes the usual ArduinoOTA listener (port `3232`) but only after the web UI asks for it. A short reboot delay gives the operator time to hit Enter in `ota.bat` so the upload can begin.

### Core Flow
1. Web UI posts to `/ota/start`.
2. `RunManager::requestArmOTA()` opens a five-minute window; `requestConfirmOTA()` schedules a reboot via `TimerManager` (15 s delay).
3. During the delay the board keeps Wi-Fi alive (`TimerManager::update()` inside the OTA loop) and then reboots.
4. After the reboot `otaBootHandler()` enables ArduinoOTA, logs `[OTA] listening on ArduinoOTA`, and waits up to five minutes for espota to connect.
5. `ota.bat` (or `pio run -e esp32_v3_ota -t nobuild -t upload`) pushes the firmware using `--auth=KwalOTA_3732`.
6. On success ArduinoOTA triggers a restart; the OTA flags in NVS are cleared automatically.

### Timing Guarantees
- **15 s countdown** between the `/ota/start` response and the reboot. This gives the operator time to hit Enter in the paused batch file.
- **5 min OTA window** after the reboot. If no upload arrives in that window the listener times out and the board resumes normal duties.

## First-Time Setup
1. Flash the latest firmware over USB so the new OTA controller is present.
2. Make sure `platformio.ini` has `--auth=KwalOTA_3732` under `upload_flags` for `esp32_v3_ota`.
3. Load the updated web assets (single Start OTA button) onto the SD card.

## Operator Checklist
1. Run `ota.bat` (builds → pauses).
2. In the web UI open **OTA** → press **Start OTA**.
3. Wait for the 15 s countdown to finish (status: “Reboot bezig – druk Enter in ota.bat”).
4. Press Enter in the batch window; espota uploads the firmware.
5. Watch the serial log for `[OTA] OK, reboot` and the new version string on boot.

## Internals
- **Preferences layout** (namespace `ota`):
	- `mode`: `0` idle, `1` enabled, `2` awaiting reboot. Always reset after success or expiry.
	- `enabled_until`: absolute seconds from `millis()/1000` when the window closes.
- **TimerManager** handles both the reboot delay and the Wi-Fi keepalive updates while ArduinoOTA is running.
- **Password**: defined in `lib/Globals/HWconfig.h` as `KwalOTA_3732`. Change both firmware and `platformio.ini` together if you rotate it.

## Failure Handling
- If upload fails (`espota` reports `Authentication Failed` or timeout), `mode` resets to `0` and run status reverts to normal.
- If the operator misses the 5 min window, simply trigger `/ota/start` again.
- Serial logs prefix with `[OTA]` for quick filtering.