# NAS CSV Extension

## Purpose
This document describes the NAS CSV extension added to the firmware. The goal is to fetch configuration CSV files over HTTP from a NAS and store them on the SD card under /nas/ without overwriting the root CSV files.

## Scope
- CSV files are fetched via HTTP GET from a configurable base URL.
- Files are stored in /nas/ on the SD card.
- Root CSV files remain untouched and serve as fallback.

## Key Concepts
- **NAS CSV**: CSV files fetched from the NAS, saved under /nas/.
- **SD CSV**: CSV files stored in the SD root (existing behavior).
- **Prefer NAS**: When reading CSV files, /nas/ is checked first. If not present, SD root is used.

## Configuration
Configuration values are defined in [lib/Globals/Globals.h](../../lib/Globals/Globals.h):
- `Globals::csvBaseUrl` (default: http://192.168.2.23:8080/csv/)
- `Globals::csvHttpTimeoutMs`
- `Globals::csvFetchWaitMs`

These values can be overridden at runtime via /config/globals.csv.

## Boot Flow
1. SD boot initializes SD access.
2. WiFi boot begins and tries to fetch CSV files from the NAS.
3. Downloads are written to temporary files in /nas/ and committed via rename.
4. If download completes (any file succeeds), the system continues.
5. If the fetch times out, the system continues with SD fallback.
6. After WiFi boot, CSV-dependent subsystems are started.

## Download Behavior
- Each CSV file is requested from `Globals::csvBaseUrl + filename`.
- Download is written to `/nas/<filename>.tmp`.
- On success, the temp file is renamed to `/nas/<filename>`.
- On failure for a specific file, any stale `/nas/<filename>` is deleted.

## CSV List
The NAS fetch uses the same CSV set as SD:
- globals.csv
- calendar.csv
- light_patterns.csv
- light_colors.csv
- theme_boxes.csv
- audioShifts.csv
- colorsShifts.csv
- patternShifts.csv

## Logging
WiFi boot logs include:
- HTTP begin failures
- HTTP status errors
- Empty downloads
- Temp rename failures
- Success lines with bytes written

## Operational Notes
- /nas/ is created automatically if missing.
- If SD is not ready or is busy, the fetch is skipped and SD fallback is used.
- No HTTPS and no authentication are supported by design.

## Files Touched
- WiFi boot: download and timeout logic
- SD path utilities: prefer /nas/ for reads
- CSV loaders: use prefer NAS then SD
- Globals: new configuration values
