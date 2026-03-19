# NAS CSV Extension

> Version: 260319A | Updated: 2026-03-19

**This document is superseded by [NAS_Fallback/readme.md](../NAS_Fallback/readme.md) (260316K).**

The original NAS extension stored fetched CSVs in a `/nas/` subdirectory and checked `/nas/` before SD root. This has been replaced by the NAS Fallback design which downloads directly to SD root (overwriting) using a `.tmp` + rename pattern.

## Key differences from this original doc

| Aspect | Old (this doc) | Current (NAS_Fallback) |
|--------|---------------|----------------------|
| Storage path | `/nas/<file>` | SD root `/<file>` |
| CSV list | 8 files (all CSVs) | 6 files (`light_patterns.csv` and `light_colors.csv` excluded) |
| Fallback | Prefer `/nas/`, fall back to root | Direct overwrite, no dual-path |

## Configuration

Still valid — see `Globals.h`:
- `Globals::csvBaseUrl` (default: `http://192.168.2.23:8081/csv/`)
- `Globals::csvHttpTimeoutMs`
- `Globals::csvFetchWaitMs`

## Implementation

Code lives in `lib/WiFiController/`:
- `FetchController.h/.cpp` — HTTP fetch coordinator
- `FetchManager.cpp` — Download lifecycle
- `NasBackup.h/.cpp` — Push CSVs back to NAS after save
