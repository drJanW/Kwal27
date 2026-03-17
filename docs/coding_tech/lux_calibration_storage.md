# Lux Calibration Parameter Storage

## Problem

The lux calibration model `y = brMax * (1 - exp(-luxRate * lux))` has three
fitted parameters: `brMax`, `luxRate`, and `luxMax`. These are **per-device** —
each jellyfish has different optics, LED characteristics, and sensor placement.

Previously these were stored in `globals.csv`. This caused a critical bug:
WiFiBoot fetches `globals.csv` from the NAS CSV server at every boot,
**replacing** the device copy with the project-wide template. Any calibration
values appended by `saveFittedParams()` were lost on reboot.

## Solution: Store in config.txt

`/config.txt` is the per-device configuration file. Each physical device
(HOUT, MARMER) has its own copy on SD. It is:

- **Never in the NAS CSV fetch list** (`WiFiBoot.cpp` `csvFiles[]`)
- **Never overwritten** by `upload_csv.ps1`
- **Read at boot** in `Globals::begin()` Step 2, before `globals.csv` Step 3

### config.txt format

Simple `key=value` pairs, one per line. Lines starting with `#` are comments.

```
# Device configuratie — per kwal uniek
name=MARMER
ssid=keijebijter
password=...
ip=192.168.2.188
gateway=192.168.2.254
rtc=1
lux=1
distance=0
sensor3=0
brMax=179.9
luxRate=0.1556
luxMax=300.0
```

## Boot Flow

```
Step 1: NVS (flash)     → device identity baseline
Step 2: config.txt (SD) → overrides NVS + loads brMax/luxRate/luxMax
Step 3: globals.csv (SD) → project-wide overrides (does NOT touch brMax/luxRate/luxMax)
```

If config.txt has no calibration keys → Globals.h compiled defaults apply
(brMax=222.0, luxRate=0.02, luxMax=300.0).

## Write Flow (Accept Fit)

1. User takes samples in Cal Lux modal
2. JS-side Gauss-Newton fit computes new brMax/luxRate
3. User clicks ✅ (approve) → `POST /api/lux/fit` + `POST /api/lux/accept`
4. `routeAcceptFit()` sets Globals in RAM, calls `saveFittedParams()`
5. `saveFittedParams()` does read-modify-write on `/config.txt`:
   - Reads all lines
   - Updates existing `brMax=` / `luxRate=` / `luxMax=` lines in-place
   - Appends any missing keys
   - Writes entire file back

## WebGUI Actions

| Icon | Route | Action |
|------|-------|--------|
| 🗑️ | `POST /api/lux/clear` | Remove samples only, keep seeds intact. Saves updated luxcal.csv. |
| 🔄 | `POST /api/lux/reset` | Regenerate seeds from current Globals.h defaults (222/0.02). Full factory reset of calibration. |
| ✅ | `POST /api/lux/accept` | Apply fitted params to RAM + persist to config.txt |

## Why NOT globals.csv?

| Property | globals.csv | config.txt |
|----------|-------------|------------|
| Scope | Project-wide (shared) | Per-device |
| NAS fetch at boot | **Yes** — overwritten | **No** — untouched |
| upload_csv.ps1 | **Yes** — overwritten | **No** — untouched |
| Format | `key;type;value;comment` (CSV) | `key=value` (simple) |
| Calibration safe? | **No** | **Yes** |

## Files Involved

- `lib/Globals/Globals.cpp` — `loadConfigTxt()` reads brMax/luxRate/luxMax
- `lib/Globals/Globals.cpp` — `applyOverride()` no longer handles brMax/luxRate/luxMax
- `lib/RunManager/Light/LuxCalibration.cpp` — `saveFittedParams()` writes to config.txt
- `lib/WebInterfaceController/routes/LuxCalRoutes.cpp` — `routeClear()`, `routeReset()`, `routeAcceptFit()`
- `sdroot/HOUT/config.txt` / `sdroot/MARMER/config.txt` — device templates
