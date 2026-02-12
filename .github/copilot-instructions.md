# Kwal27 ESP32 Firmware - Copilot Instructions

## Project Overview
ESP32 ambient art installation (jellyfish sculpture) with LED lights, audio playback, web interface, and sensor integration. PlatformIO project using Arduino framework with ESP32-S3, 16MB flash.

## Architecture: Boot → Plan → Policy → Run → Controller

```
TimerManager (central timing) → RunManager (orchestration)
    ↓                              ↓
Boot layers (one-time init)    Policy layers (rules/constraints)
    ↓                              ↓
Controllers (hardware APIs)    Directors (context→requests)
```

- **Boot**: One-time initialization, register timers, seed caches
- **Run**: Owns timer callbacks (`cb_*` prefix), sequences work, raises requests
- **Policy**: Domain rules, approve/deny requests - NO side effects, NO timers
- **Director**: Build requests from context - NO policy decisions
- **Controller**: Hardware drivers (FastLED, I2S, SPI) - ONLY controllers touch hardware

## Critical Rules

### Decision Discipline
- You may question me, please do.
- Never ignore a direct request.
- Never reverse a decision without asking first.

### Engineering Discipline
- Avoid quick fixes and patch chains. Prefer a structural solution based on the actual boot/run sequence.
- Trace the flow end-to-end before editing when symptoms are timing/order related.

### Terminology Discipline
- Never introduce new terms as synonyms. If a term exists, reuse it.
- New terms must mean new concepts and require explicit approval.

### Timing
- **ONLY use TimerManager** — this is the project's own timer API (`timers.create()`, `timers.restart()`, `timers.cancel()`)
- **NEVER** use Arduino/ESP timers, `millis()`, `delay()`, `esp_timer`, `Ticker`, or any other timing mechanism
- Callbacks: prefix with `cb_`, execute actions, NO polling or "if ready" checks
- `create()` = new timer, `restart()` = reuse existing with same callback
- For repeating work use `create(interval, 0, cb)` — repeat count 0 = infinite
- **Never self-reschedule** inside a callback (no `restart()` at end of `cb_*`)

### Version Bumping (BEFORE any code change)
- Firmware: [lib/Globals/Globals.h](../lib/Globals/Globals.h) → `FIRMWARE_VERSION`
- WebGUI JS: [sdroot/webgui-src/build.ps1](../sdroot/webgui-src/build.ps1) → `$version` **ONLY when WebGUI sources change**
- **Per-file headers**: When editing any `.h` or `.cpp` file, update its `@version` and `@date` in the file's Doxygen header to today's date

### WebGUI Pipeline (JS is NOT auto-built)
```powershell
# Edit sources in sdroot/webgui-src/js/*.js (NEVER edit sdroot/kwal.js directly)
cd sdroot\webgui-src
.\build.ps1                    # Concatenates to ../kwal.js
cd ..\..
.\upload_web.ps1               # Uploads to ESP32 SD card
```

### Include Order
Every `.cpp` file: `#include <Arduino.h>` as FIRST include

## Code Style

- **camelCase** for all identifiers (never snake_case)
- Callbacks: `cb_verbNoun()` pattern
- Functions: `verbNoun()` (e.g., `calcBaseHi`, `applyBrightnessRules`)
- **English only** in all `.cpp`, `.h`, `.md` files and code comments

### Terminology (use these exact terms)
| Use | NOT |
|-----|-----|
| interval | cadence, period |
| fraction (0.0-1.0) | factor, mult |
| multiplier (0.0+) | modifier |
| swing (-1.0 to +1.0) | scale, strength |
| shift | offset |
| brightness | bri, bright |
| volume | gain (except I2S registers) |
| speak | speech |
| create | schedule |

## Key Directories

| Path | Purpose |
|------|---------|
| `lib/ConductManager/` | Boot/Run/Policy/Director layers per subsystem |
| `lib/TimerManager/` | Central non-blocking timer system (60 slots) |
| `lib/Globals/` | Shared config, `Globals.h`, `HWconfig.h`, `macros.inc` |
| `sdroot/` | SD card content: web assets, CSV configs |
| `sdroot/webgui-src/js/` | JS source modules → built to `kwal.js` |
| `sdroot/webgui-src/build.ps1` | JS build script only — does NOT touch HTML or CSS |
| `A56/` | Staging area for work-in-progress refactors |
| `docs/` | Design docs, `glossary_slider_semantics.md` (read before brightness work) |

## Build & Deploy Commands

```powershell
.\deploy.ps1 hout     # USB upload to HOUT (189)
.\deploy.ps1 marmer   # OTA upload to MARMER (188)
.\ota.ps1 [188|189]   # Standalone OTA update
.\trace.ps1 [COM#]    # Serial monitor
.\upload_web.ps1      # Upload web files to SD
.\upload_csv.ps1      # Upload CSV configs to SD
.\download_csv.ps1    # Download CSVs from device
.\naspush.ps1 "msg"   # Commit and push to NAS
```

## Before Editing Code

1. **Search for existing solutions** - check `can*()`, `is*()`, `set*()`, `get*()` functions
2. **Check source vs generated** - `kwal.js` is generated from `webgui-src/js/`
3. **Read glossary** if touching brightness/slider logic
4. **Grep for guards** if values don't propagate: `#ifdef|DISABLE|SKIP|return;`

### Network & I/O in Callbacks
- **Never block** in a timer callback — no HTTP calls with long timeouts, no synchronous SD writes that can stall
- HTTP timeouts in callbacks: **max 1.5 seconds** — the main loop must keep running
- Always guard network calls: check WiFi connected, target reachable, SD free, audio idle BEFORE starting I/O
- If a network target is unreachable, **fail fast and retry later** — never spin or wait

### Responsibility Separation
- **Web handlers** ONLY set state in memory (variables, flags). NEVER do SD I/O or network I/O from a web handler.
- SD writes happen ONLY in timer callbacks (`cb_*`) that check `isSdBusy()` first
- A subsystem must NOT call into an unrelated subsystem (e.g., audio must not call voting, lighting must not call audio)
- If two subsystems need coordination, they communicate through shared state or RunManager — not direct calls

### Implementation Discipline
- Start minimal. Add complexity only when a real problem demands it.
- No speculative guards, no "just in case" logging, no defensive wrappers around already-validated data
- One feature = one responsibility. If a function touches two domains, split it.
- When adding a new feature, write the **smallest possible version first**, deploy, test, then iterate

### WebGUI File Discipline
- **Deployed files** = `sdroot/index.html`, `sdroot/styles.css` — edit THESE directly
- **Never edit** `webgui-src/index.html` or `webgui-src/styles.css` — they are reference copies, NOT used by the build
- `build.ps1` only processes `webgui-src/js/*.js` → `sdroot/kwal.js` (and updates cache-bust in `sdroot/index.html`)

### CSS Specificity in Modals
- Before adding component styles inside `.modal-box`, **check existing global selectors** that will override them
- Known traps: `.modal-box button` (0,1,1) overrides any class-only button selector (0,1,0) — sets `display:block; width:100%`
- Known traps: global `input[type="range"]` pseudo-elements override scoped slider styles
- Fix: use `!important` on critical properties, or prefix with ancestor class (`.mg-card .mg-sliderrow input[type="range"]`) for higher specificity
- **Never invent alternative layouts** when reference/mock code exists — copy the exact structure and fix specificity conflicts

## What NOT to Do

- Never run `pio` commands directly - say "Nu compileren" and wait
- Never edit `sdroot/kwal.js` - edit `sdroot/webgui-src/js/*.js` sources
- Never edit `sdroot/webgui-src/index.html` or `sdroot/webgui-src/styles.css` — they are NOT build inputs
- Never add "safety" guards that duplicate upstream validation
- Never use generic verbs (handle, process, do) - be specific (report, speak, update)
- Never skip version bump before code changes
- Never put SD/network I/O in a web request handler — defer to a timer callback
- Never make blocking HTTP calls from timer callbacks — use short timeouts and fail fast
- Never call across unrelated subsystems directly (audio↔voting, lighting↔network)
