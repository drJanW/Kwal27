# Kwal27 ESP32 Firmware - Copilot Instructions

ESP32 ambient art installation (jellyfish sculpture) with LED lights, audio playback, web interface, and sensor integration. PlatformIO project using Arduino framework with ESP32-S3, 16MB flash.

---

## HARD STOPS — rules 1-7 are absolute, never break them

1. **Version bump FIRST** — bump before ANY firmware or WebGUI code edit.
   - **Firmware**: `lib/Globals/Globals.h` line 17 → `#define FIRMWARE_VERSION_CODE "YYMMDDX"`
   - **WebGUI JS**: `sdroot/webgui-src/build.ps1` line 25 → `$version = "YYMMDDX"` (only when JS sources change)
   - **Per-file headers**: update `@version` and `@date` in every edited `.h`/`.cpp` Doxygen header
2. **Never edit `sdroot/kwal.js`** — edit sources in `sdroot/webgui-src/js/*.js`, then run `build.ps1`.
3. **Never run `pio` or `deploy.ps1`** — after firmware changes say **"Ik heb firmware gebumped. Nu compileren!"** and stop.
4. **Never upload to 188 (MARMER)** — unless the user explicitly says so. HOUT (189) is the safe target.
5. **Only TimerManager for timing** — no `millis()`, `delay()`, `esp_timer`, `Ticker`, or any other timing mechanism.
6. **Web handlers: memory only** — no SD I/O, no network I/O, no blocking calls from a web handler. Defer work to timer callbacks.
7. **Never run `upload_csv.ps1`** — CSV uploads are the user's responsibility.

---

## Decision Discipline

- You may question me, please do.
- Never ignore a direct request.
- Never reverse a decision without asking first.
- After ~30 exchanges: warn "Chat wordt lang, overweeg nieuw gesprek."
- After ~50 exchanges: warn again more urgently.

---

## Before Editing Code — checklist

Run through this every time before making changes:

- [ ] **Version bumped?** (see hard stop 1)
- [ ] **Searched for existing mechanism?** — grep for `can*()`, `is*()`, `set*()`, `get*()` before writing new code
- [ ] **Used existing utilities?** — check MathUtils.h (below) before writing any math/clamping/mapping logic
- [ ] **Timer pattern correct?** — check TimerManager cheatsheet (below) before writing any timer code
- [ ] **Source vs generated?** — if touching web: edit `webgui-src/js/`, never `kwal.js` directly
- [ ] **Right layer?** — `cb_*` only in `lib/RunManager/`. Web handlers only set flags. Policy has no side effects.
- [ ] **Glossary checked?** — read `docs/glossary_slider_semantics.md` before brightness/slider work
- [ ] **Guards checked?** — if values don't propagate, grep for `#ifdef|DISABLE|SKIP|return;`

---

## Architecture

```
TimerManager (central timing) → RunManager (orchestration)
    ↓                              ↓
Boot layers (one-time init)    Policy layers (rules/constraints)
    ↓                              ↓
Controllers (hardware APIs)    Directors (context→requests)
```

| Layer | Role | Constraints |
|-------|------|-------------|
| **Boot** | One-time init, register timers, seed caches | Runs once at startup |
| **Run** (`lib/RunManager/`) | Owns `cb_*` callbacks, sequences work, raises requests | ONLY place for `cb_*` functions |
| **Policy** | Domain rules, approve/deny requests | NO side effects, NO timers, NO hardware |
| **Director** | Build requests from context | NO policy decisions |
| **Controller** | Hardware drivers (FastLED, I2S, SPI) | ONLY controllers touch hardware |

### Anti-Shortcut Rule
When adding new functionality:
1. FIRST find the existing mechanism that does the same or similar thing
2. THEN extend it with a public API if needed
3. NEVER create a parallel mechanism in a different layer
4. If you catch yourself writing `cb_*` outside `lib/RunManager/` — STOP. Wrong layer.
5. Avoid treating symptoms — find and fix the root cause.

### Responsibility Separation
- A subsystem must NOT call into an unrelated subsystem (e.g., audio must not call voting, lighting must not call audio)
- If two subsystems need coordination, they communicate through shared state or RunManager — not direct calls
- Web handlers ONLY set state in memory (variables, flags) — then a `cb_*` callback does the actual work
- SD writes happen ONLY in timer callbacks that check `isSdBusy()` first

---

## TimerManager — cheatsheet

### API
```cpp
bool create(uint32_t interval, uint8_t repeat, TimerCallback cb, float growth = 1.0f, uint8_t token = 1);
bool restart(uint32_t interval, uint8_t repeat, TimerCallback cb, float growth = 1.0f, uint8_t token = 1);
void cancel(TimerCallback cb, uint8_t token = 1);
bool isActive(TimerCallback cb, uint8_t token = 1) const;
uint8_t remaining() const;  // only valid inside a callback
```

### Parameters
- **`interval`** — milliseconds. Use `SECONDS(n)`, `MINUTES(n)`, `HOURS(n)` macros.
- **`repeat`** — `0` = infinite, `1` = one-shot, `N` = exactly N fires then auto-free.
- **`growth`** — multiplier on interval after each fire. `1.0` = constant, `2.0` = exponential backoff.
- **`token`** — differentiates multiple timers with the same callback. Identity = `(cb, token)`.
- **`create()`** fails if `(cb, token)` already active. **`restart()`** = cancel + create, never fails for that reason.

### Pick the right pattern
| Need | Pattern | WRONG approach |
|------|---------|----------------|
| Run every N ms forever | `create(N, 0, cb)` | `restart()` at end of callback |
| Run once after delay | `create(delay, 1, cb)` | polling with `millis()` |
| Retry up to N times | `create(interval, N, cb)` | manual counter + restart |
| Retry with backoff | `create(startMs, N, cb, 2.0)` | manual counter + increasing delay |
| Varying interval each fire | `restart(newMs, 1, cb)` inside cb | only valid case for self-reschedule |
| Cancel when done | `timers.cancel(cb)` inside cb | returning early forever |

### Self-reschedule rule
- **ONLY** use `restart()` inside a callback when the next interval genuinely varies (random, dynamic calculation)
- If the interval is constant → use `repeat=0` (infinite), NEVER `restart()` with the same interval

### Network & I/O in Callbacks
- **Never block** — no HTTP calls with long timeouts, no synchronous SD writes
- HTTP timeouts: **max 1.5 seconds**
- Guard network calls: check WiFi connected, target reachable, SD free, audio idle BEFORE starting I/O
- Unreachable target → **fail fast, retry later** — never spin or wait

---

## Build & Deploy

### Devices
| Device | IP | Role | Access |
|--------|----|------|--------|
| **HOUT** | 192.168.2.189 | Sandbox / dev | USB (COM12), physical SD |
| **MARMER** | 192.168.2.188 | Production | OTA only, physically inaccessible |

### Who does what
| Actor | Runs | Never runs |
|-------|------|------------|
| **Copilot** | `build.ps1`, `upload_web.ps1 192.168.2.189` | `pio`, `deploy.ps1`, `upload_csv.ps1`, anything to 188 |
| **User** | `deploy.ps1`, `ota.ps1`, `upload_csv.ps1` | — |

### WebGUI pipeline
```powershell
cd sdroot\webgui-src
.\build.ps1                    # webgui-src/js/*.js → sdroot/kwal.js (+ cache-bust)
cd ..\..
.\upload_web.ps1 192.168.2.189 # Upload to HOUT only
```

### WebGUI file rules
- **Edit directly**: `sdroot/index.html`, `sdroot/styles.css`
- **Never edit**: `webgui-src/index.html`, `webgui-src/styles.css` (reference copies, not build inputs)
- `build.ps1` only processes `webgui-src/js/*.js` → `sdroot/kwal.js`

### Other commands
```powershell
.\deploy.ps1 hout              # USB upload firmware to HOUT
.\deploy.ps1 marmer            # OTA upload firmware to MARMER
.\ota.ps1 [188|189]            # Standalone OTA
.\trace.ps1 [COM#]             # Serial monitor
.\upload_csv.ps1               # Upload CSV configs (user only)
.\download_csv.ps1             # Download CSVs from device
.\naspush.ps1 "msg"            # Commit and push to NAS
```

---

## Code Style

- **camelCase** for all identifiers (never snake_case)
- Callbacks: `cb_verbNoun()` pattern
- Functions: `verbNoun()` (e.g., `calcBaseHi`, `applyBrightnessRules`)
- **English only** in all `.cpp`, `.h`, `.md` files and code comments
- Every `.cpp` file: `#include <Arduino.h>` as FIRST include
- Never use generic verbs (handle, process, do) — be specific (report, speak, update)

### Terminology — use these exact terms
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

- Never introduce new terms as synonyms. If a term exists, reuse it.
- New terms must mean new concepts and require explicit approval.

---

## MathUtils — use these, never rewrite them

`#include "MathUtils.h"` — available project-wide.

| Function | What it does | Use instead of |
|----------|-------------|----------------|
| `clamp(val, lo, hi)` | Clamp to range; auto-swaps if lo > hi | `if (val < lo) val = lo; if (val > hi) val = hi;` |
| `clamp01(val)` | Clamp float to [0.0, 1.0] | `constrain(val, 0.0, 1.0)` or manual if/then |
| `map(val, inLo, inHi, outLo, outHi)` | Linear map, **unclamped** output | `Arduino map()` or manual formula |
| `mapRange(val, inLo, inHi, outLo, outHi)` | Linear map, **clamped** output | `map()` + `constrain()` combo |
| `lerp(a, b, t)` | Interpolate a→b by t (clamped to [0,1]) | `a + (b - a) * t` with manual clamp |
| `inverseLerp(a, b, val)` | Where does val fall in [a,b]? → [0,1] | manual `(val - a) / (b - a)` |
| `wrap(val, lo, hi)` | Modular wrap into range | manual modulo |
| `wrap01(val)` | Wrap to [0.0, 1.0) | `fmod()` |
| `nearlyEqual(a, b, eps)` | Float comparison within epsilon | `abs(a - b) < 0.001` |
| `minVal(a, b)` / `maxVal(a, b)` | Type-safe min/max (no double-eval) | Arduino `min()`/`max()` macros |
| `applyDeadband(val, thresh)` | Zero below threshold | manual `if (abs(val) < thresh) return 0` |
| `applyHysteresis(cur, val, on, off)` | Hysteresis toggle | manual if/else state tracking |

**Rule: if you're about to write an `if` chain that clamps, maps, interpolates, or compares floats — stop and use MathUtils.**

---

## Engineering Discipline

- **Use existing utilities FIRST** — before writing any logic, grep the codebase for functions that already do what you need.
- Start minimal. Add complexity only when a real problem demands it.
- No speculative guards, no "just in case" logging, no defensive wrappers around already-validated data.
- One feature = one responsibility. If a function touches two domains, split it.
- Smallest possible version first → deploy → test → iterate.
- Avoid quick fixes and patch chains. Prefer structural solutions based on the actual boot/run sequence.
- Trace the flow end-to-end before editing when symptoms are timing/order related.

### CSS Specificity in Modals
- Before adding styles inside `.modal-box`, check existing global selectors that override them
- Traps: `.modal-box button` (0,1,1) overrides class-only selectors (0,1,0); global `input[type="range"]` pseudo-elements override scoped slider styles
- Fix: `!important` on critical properties, or prefix with ancestor class for higher specificity
- Never invent alternative layouts when reference/mock code exists — copy the exact structure

---

## Key Directories

| Path | Purpose |
|------|---------|
| `lib/RunManager/` | Run layer: timer callbacks (`cb_*`), orchestration |
| `lib/TimerManager/` | Central non-blocking timer system (60 slots) |
| `lib/Globals/` | Shared config: `Globals.h`, `HWconfig.h`, `macros.inc` |
| `sdroot/` | SD card content: web assets, CSV configs |
| `sdroot/webgui-src/js/` | JS source modules → built to `kwal.js` |
| `sdroot/webgui-src/build.ps1` | JS build script — does NOT touch HTML or CSS |
| `A56/` | Staging area for work-in-progress refactors |
| `docs/` | Design docs, `glossary_slider_semantics.md` |
