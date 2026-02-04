# NAS CSV Read and Fallback

## Purpose
This document explains how CSV reading prefers NAS files and how fallback to SD root works.

## Read Order
When a CSV file is needed, the firmware uses this order:
1. `/nas/<filename>` if it exists
2. `/<filename>` from SD root if `/nas/` file is missing

This is implemented through `chooseCsvPath()`.

## Fallback Rules
Fallback is based on the presence of files, not on runtime errors during parsing:
- If `/nas/<filename>` exists, it is used.
- If it does not exist, SD root is used.

## Failure Cleanup
To ensure correct fallback behavior after failed downloads:
- If a NAS download fails for a file, the existing `/nas/<filename>` is deleted.
- This prevents stale NAS files from blocking SD root fallback.

## Timeout Behavior
If the overall NAS fetch waits too long:
- A timeout triggers continuation of boot.
- At that point, any missing `/nas/` files will fall back to SD root.

## What This Means In Practice
- Fresh NAS files override SD root files only when the download succeeds.
- SD root files remain the safe fallback at all times.
- A partial NAS fetch still allows fallback per-file.

## Loader Coverage
All CSV loaders now use the same `chooseCsvPath()` behavior:
- Calendar
- Theme boxes
- Light patterns
- Light colors
- Pattern shifts
- Color shifts
- Audio shifts
- Globals

## Common Troubleshooting
- If SD fallback is not happening, check for stale files in /nas/.
- If NAS fetch never runs, verify WiFi boot and `Globals::csvBaseUrl`.
- If files are empty, check NAS HTTP server and CSV content.
