# NAS CSV Download

> Version: 260316K | Updated: 2026-03-16

## Purpose
At boot, the firmware downloads CSV configuration files from the NAS
directly to SD root, overwriting the existing copies.

## Downloaded CSVs (6)
- globals.csv
- calendar.csv
- theme_boxes.csv
- audioShifts.csv
- colorsShifts.csv
- patternShifts.csv

## Excluded from NAS download (device-specific)
- **light_patterns.csv** — contains PNF calibrations written by the ESP32
- **light_colors.csv** — device-specific

These files are only updated via WebGUI upload or direct SD write.

## Download Flow
1. WiFi connects, NAS health check passes
2. Each CSV is downloaded to `/<filename>.tmp`
3. On success: `.tmp` is renamed to `/<filename>` (atomic overwrite)
4. On failure: `.tmp` is deleted, existing root file stays untouched

## Fallback
- NAS unreachable → existing SD root files are used as-is
- Boot patience timer ensures boot continues if NAS is slow

## Common Troubleshooting
- If NAS fetch never runs, verify WiFi boot and `Globals::csvBaseUrl`.
- If files are empty, check NAS HTTP server and CSV content.
