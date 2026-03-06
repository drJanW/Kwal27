# TV Simulator — Integrated Mode (v3)

## Concept

TV mode runs **inside the normal firmware** as a runtime overlay.
A one-shot timer (`cb_tvScene`) calls `PlayLightShow()` every 250-2500ms with
random 2-color CircleShow params. Audio locks to dir 144. That's it.

- **`Globals::tvMode`** — RAM bool (default `false`). Power loss → always safe.
- **No custom render.** Standard CircleShow renders the random params.
- **No guards.** `cb_tvScene` overwrites any external change within 2.5s.
- **No new files.** ~90 lines across existing files.

### Design philosophy

| Principle | Implementation |
|-----------|---------------|
| Use existing mechanism | `PlayLightShow(LightShowParams(...))` — same as every pattern |
| No guards needed | `cb_tvScene` fires every 0.25-2.5s, overwrites anything |
| Calendar at midnight | May change pattern/colors — cb_tvScene overwrites within 2.5s. If user sleeps and timer expires, `exitTvMode()` restores. By design. |
| Audio/TTS unblocked | `cb_sayTime`, `cb_playFragment` run normally — no interference |
| Minimal state | 1 bool (`tvMode`) + 1 uint8_t (`tvHue`) |

---

## Architecture

```
Normal Firmware (always running)
┌──────────────────────────────────────────────────────────┐
│                                                          │
│  WebGUI: [📺 TV] button + hours slider                  │
│    → GET /api/tvmode?hours=N → enterTvMode(N)            │
│    → GET /api/tvstop         → exitTvMode()              │
│                                                          │
│  ┌─ Globals::tvMode ─────────────────────────────────┐   │
│  │                                                   │   │
│  │  cb_tvScene (one-shot, self-rescheduling):        │   │
│  │    pick 2 random HSV colors → CRGB                │   │
│  │    PlayLightShow(LightShowParams(a, b, ...))      │   │
│  │    reschedule 250-2500ms                          │   │
│  │                                                   │   │
│  │  Audio: requestSetSingleDirThemeBox(144)          │   │
│  │    existing cb_playFragment runs with 2-5s gaps   │   │
│  │                                                   │   │
│  │  No guards — everything else runs normally.       │   │
│  │  cb_tvScene overwrites any change within 2.5s.    │   │
│  │                                                   │   │
│  │  cb_tvTimeout (Nh, 1×) → exitTvMode()             │   │
│  └───────────────────────────────────────────────────┘   │
│                                                          │
│  Power loss → RAM cleared → tvMode=false → normal boot   │
│  Calendar midnight → may change lights → overwritten     │
│                                                          │
└──────────────────────────────────────────────────────────┘
```

---

## State

```cpp
// In Globals.h
inline static bool tvMode = false;

// In RunManager.cpp (file scope)
static uint8_t  tvHue = 0;               // Hue drift across scenes
static uint32_t tvSavedSingleDirMin;      // Backup audio intervals
static uint32_t tvSavedSingleDirMax;
```

---

## The Three Functions

Everything lives in `RunManager.cpp` (orchestration layer).

### enterTvMode

```cpp
void enterTvMode(uint8_t hours) {
    Globals::tvMode = true;

    // Save + tighten audio intervals
    tvSavedSingleDirMin = Globals::singleDirMinIntervalMs;
    tvSavedSingleDirMax = Globals::singleDirMaxIntervalMs;
    Globals::singleDirMinIntervalMs = 2000;
    Globals::singleDirMaxIntervalMs = 5000;

    // Lock audio to TV directory
    requestSetSingleDirThemeBox(TvConfig::audioDir);

    // LED brightness for TV effect
    FastLED.setBrightness(TvConfig::maxBrightness);

    // Seed hue and start scene timer
    tvHue = random(256);
    cb_tvScene();  // First scene immediately

    // Auto-exit after N hours
    timers.create(static_cast<uint32_t>(hours) * 3600000UL, 1, cb_tvTimeout);

    PF("[TvSim] Started for %u hours\n", hours);
}
```

### exitTvMode

```cpp
void exitTvMode() {
    Globals::tvMode = false;

    // Restore audio
    Globals::singleDirMinIntervalMs = tvSavedSingleDirMin;
    Globals::singleDirMaxIntervalMs = tvSavedSingleDirMax;
    AudioPolicy::resetToBaseThemeBox();

    // Restore LEDs (same as AlertRGB uses)
    LightRun::reapplyCurrentShow();

    // Cancel timers
    timers.cancel(cb_tvScene);
    timers.cancel(cb_tvTimeout);

    PL("[TvSim] Stopped");
}

void cb_tvTimeout() {
    exitTvMode();
}
```

### cb_tvScene — the heart of the feature

```cpp
void cb_tvScene() {
    // Hue drift: wander ±60 from previous scene
    uint8_t drift = random(0, 60);
    tvHue += random(2) ? drift : (256 - drift);

    // Two random HSV colors → CRGB
    CRGB a = CHSV(tvHue,                     random(100, 200), random(180, 250));
    CRGB b = CHSV(tvHue + random(40, 120),   random(80, 200),  random(160, 250));

    // Random CircleShow params for this scene
    PlayLightShow(LightShowParams(
        a, b,
        100, 100,                   // colorCycleSec, brightCycleSec (long → static within scene)
        (float)random(4, 16),       // fadeWidth (sharp to soft zone edge)
        random(30, 80),             // minBrightness
        0.0f,                       // gradientSpeed (no radius animation)
        (float)random(-5, 6),       // centerX (slight offset per scene)
        (float)random(-5, 6),       // centerY
        (float)random(3, 20),       // radius (zone size)
        random(2, 12),              // windowWidth (color spread)
        0.0f,                       // radiusOsc (static)
        0.0f, 0.0f,                 // xAmp, yAmp (no center movement)
        100, 100                    // xCycleSec, yCycleSec
    ));

    // Reschedule: next scene in 250-2500ms
    timers.create(random(250, 2500), 1, cb_tvScene);
}
```

---

## TV Scene Parameters — what the "patterns" and "colors" look like

Each `cb_tvScene` call creates a **unique CircleShow configuration**. Here's what
each randomized parameter does visually:

### Colors (2 per scene)

| Parameter | Range | Visual effect |
|-----------|-------|---------------|
| `tvHue` | 0-255, drifts ±60 | Base hue wanders — like TV scenes changing mood |
| Color A saturation | 100-200 | Rich but not neon |
| Color A value | 180-250 | Bright (TV is a light source) |
| Color B hue offset | +40 to +120 from A | Always distinct from A — visible 2-color zones |
| Color B saturation | 80-200 | Can be slightly washed out |
| Color B value | 160-250 | Slightly dimmer allowed |

**Example scenes the algorithm would produce:**

| Scene | Color A | Color B | Mood |
|-------|---------|---------|------|
| 1 | Warm white (hue~40, sat~100, val~240) | Soft blue (hue~160, sat~140, val~200) | News anchor |
| 2 | Deep blue (hue~160, sat~180, val~200) | Teal (hue~220, sat~120, val~220) | Night scene |
| 3 | Orange (hue~25, sat~190, val~250) | Yellow (hue~70, sat~100, val~240) | Explosion/fire |
| 4 | Purple (hue~200, sat~160, val~190) | Pink (hue~240, sat~140, val~230) | Sunset scene |
| 5 | Green (hue~90, sat~170, val~210) | Cyan (hue~140, sat~100, val~240) | Forest/nature |

### Spatial layout (CircleShow params)

| Parameter | Range | Visual effect |
|-----------|-------|---------------|
| `fadeWidth` | 4-16 | **Sharp edge** (4) = distinct color blobs. **Soft** (16) = gentle blend. |
| `radius` | 3-20 | **Small** (3) = many color rings. **Large** (20) = big single-color zones. |
| `windowWidth` | 2-12 | **Narrow** (2) = binary 2-color. **Wide** (12) = gradient spread. |
| `centerX/Y` | -5 to +5 | Pattern shifts slightly each scene (like TV image moving) |
| `minBrightness` | 30-80 | LEDs far from center aren't fully black |

**LED coordinate system:** Fallback is a circle with radius ≈12.6 (sqrt(160)).
With custom ledmap loaded, coordinates may differ. The param ranges above work
for both — `radius` 3-20 covers small-to-full-coverage in either layout.

### Scene timing

| Parameter | Range | Visual effect |
|-----------|-------|---------------|
| Scene duration | 250-2500ms | Fast scenes (~250ms) = channel surfing. Slow (~2500ms) = drama |
| Transition | Instant (PlayLightShow) | CircleShow applies new params on next 50ms render tick |

**Note:** No explicit crossfade logic needed. `PlayLightShow()` instantly sets new
params, and the existing 50ms render applies them. At 20fps with random hue drift, the
visual result is indistinguishable from explicit crossfades through a window at distance.

---

## Audio

### Entering TV mode — 1 function call

```cpp
requestSetSingleDirThemeBox(TvConfig::audioDir);  // dir 144
```

This **existing function** does three things:
1. `AudioPolicy::setThemeBox(&dir, 1, "web-144")` — locks fragment selection to dir 144
2. `requestPlaySpecificFragment(144, -1, "grid/dir")` — plays a random file immediately
3. `timers.restart(random(min, max), 1, cb_playFragment)` — schedules next fragment

### Tightened intervals

```cpp
Globals::singleDirMinIntervalMs = 2000;    // Was MINUTES(1)
Globals::singleDirMaxIntervalMs = 5000;    // Was MINUTES(5)
```

2-5 second gaps between tracks. Like continuous TV audio with natural pauses.

### Exit restores

```cpp
Globals::singleDirMinIntervalMs = tvSavedSingleDirMin;
Globals::singleDirMaxIntervalMs = tvSavedSingleDirMax;
AudioPolicy::resetToBaseThemeBox();
```

**Zero custom audio code.**

---

## Web Integration

### Routes — in existing `WebInterfaceController/routes/`

```cpp
#include "RunManager.h"
#include "MathUtils.h"

void routeEnterTvMode(AsyncWebServerRequest *request) {
    if (Globals::tvMode) {
        request->send(200, "application/json", "{\"active\":true}");
        return;
    }
    uint8_t hours = 4;
    if (request->hasParam("hours")) {
        hours = MathUtils::clamp(
            (uint8_t)request->getParam("hours")->value().toInt(),
            (uint8_t)1, (uint8_t)12);
    }
    enterTvMode(hours);
    request->send(200, "application/json", "{\"active\":true}");
}

void routeExitTvMode(AsyncWebServerRequest *request) {
    if (Globals::tvMode) exitTvMode();
    request->send(200, "application/json", "{\"active\":false}");
}

// In attachRoutes():
server.on("/api/tvmode", HTTP_GET, routeEnterTvMode);
server.on("/api/tvstop", HTTP_GET, routeExitTvMode);
```

### WebGUI — button + slider in existing `index.html`

```html
<div class="tv-section">
    <button id="btnTvMode" class="tv-btn" onclick="startTvMode()">📺 TV</button>
    <input type="range" id="tvHours" min="1" max="12" value="4" step="1">
    <span id="tvHoursLabel">4 uur</span>
</div>
```

### JavaScript — in `webgui-src/js/`

```javascript
document.getElementById('tvHours')?.addEventListener('input', function() {
    document.getElementById('tvHoursLabel').textContent = this.value + ' uur';
});

function startTvMode() {
    const hours = document.getElementById('tvHours')?.value || 4;
    if (!confirm('Start TV Simulator voor ' + hours + ' uur?')) return;
    fetch('/api/tvmode?hours=' + hours)
        .then(r => r.json())
        .then(d => {
            if (d.active) {
                document.getElementById('btnTvMode').textContent = '⏹ Stop TV';
                document.getElementById('btnTvMode').onclick = stopTvMode;
            }
        });
}

function stopTvMode() {
    fetch('/api/tvstop')
        .then(() => {
            document.getElementById('btnTvMode').textContent = '📺 TV';
            document.getElementById('btnTvMode').onclick = startTvMode;
        });
}
```

---

## Power-off Safety

`Globals::tvMode` lives in RAM. Power loss → cleared → normal boot. Done.

---

## Activation / Deactivation Flow

```
                    ENTER                                      EXIT
                    =====                                      ====

User taps [📺 TV] (hours=4)                       User taps [⏹ Stop TV]
    │                                                   │
    ▼                                                   ▼
GET /api/tvmode?hours=4                            GET /api/tvstop
    │                                                   │
    ▼                                                   ▼
enterTvMode(4)                                     exitTvMode()
    │                                                   │
    ├─ tvMode = true                                    ├─ tvMode = false
    ├─ Save + tighten audio intervals                   ├─ Restore audio intervals
    ├─ requestSetSingleDirThemeBox(144)                 ├─ resetToBaseThemeBox()
    ├─ tvHue = random, cb_tvScene()                     ├─ reapplyCurrentShow()
    ├─ timers.create(4h, 1, cb_tvTimeout)               ├─ cancel cb_tvScene + cb_tvTimeout
    └─ Done                                             └─ Done

    cb_tvScene fires every 250-2500ms:                  OR: cb_tvTimeout after N hours
        PlayLightShow(random 2-color params)                 → exitTvMode()
        reschedule self
                                                        OR: Power loss
    Everything else runs normally:                           → RAM cleared → normal boot
        cb_changeColor → runs, overwritten <2.5s
        cb_changePattern → runs, overwritten <2.5s
        cb_loadCalendar → runs (midnight = by design)
        cb_sayTime → runs normally
        cb_playFragment → runs (locked to dir 144)
```

---

## File Impact

| File | Change | Lines |
|------|--------|:-----:|
| `lib/Globals/Globals.h` | `inline static bool tvMode = false;` | 1 |
| `lib/RunManager/RunManager.h` | Declare `enterTvMode()`, `exitTvMode()` | 2 |
| `lib/RunManager/RunManager.cpp` | `enterTvMode()`, `exitTvMode()`, `cb_tvScene`, `cb_tvTimeout` + state vars | ~45 |
| `lib/WebInterfaceController/routes/*.cpp` | `/api/tvmode`, `/api/tvstop` routes | ~18 |
| `sdroot/index.html` | Button + slider HTML | 5 |
| `sdroot/webgui-src/js/*.js` | `startTvMode()`, `stopTvMode()`, slider handler | 15 |
| `sdroot/styles.css` | TV section styles | 5 |
| **Total** | **0 new files** | **~91** |

### What was removed (v2 → v3)

| Removed | Why |
|---------|-----|
| `tvFlicker()`, `tvInitScene()`, `tvNewScene()` in LightController.cpp | Replaced by `PlayLightShow()` with random params |
| `hsvToRgb768()` in LightController.cpp | Using `CHSV()` → `CRGB` (FastLED built-in) |
| `TvScene` struct in LightController.cpp | No custom render state needed |
| Guard in `updateLightController()` | Standard CircleShow renders TV params |
| Guards in `cb_changeColor`, `cb_changePattern`, `cb_loadCalendar`, `cb_sayTime` | cb_tvScene overwrites within 2.5s |
| Declarations in LightController.h | Nothing added to LightController |

### Comparison

| Metric | v3 (this plan) | v2 (custom render) | Plan B (separate boot) |
|--------|:--------------:|:------------------:|:----------------------:|
| New files | 0 | 0 | 2 |
| New lines | ~91 | ~142 | ~350 |
| Guards | 0 | 4 | 0 |
| LightController changes | 0 | ~57 | ~80 |
| Custom render code | 0 | ~55 | ~55 |

---

## Config

```cpp
namespace TvConfig {
    constexpr uint8_t  audioDir      = 144;
    constexpr uint8_t  maxBrightness = 250;
}
```

---

## What Runs in TV Mode vs Normal

| Component | Normal | TV Mode | Notes |
|-----------|:------:|:-------:|-------|
| TimerManager | ✓ | ✓ | All timers tick normally |
| updateLightController (CircleShow) | ✓ | ✓ | Renders whatever `PlayLightShow()` last set |
| **cb_tvScene** | — | ✓ | Calls `PlayLightShow()` every 250-2500ms |
| cb_changeColor | ✓ | ✓ | May fire — overwritten by cb_tvScene within 2.5s |
| cb_changePattern | ✓ | ✓ | May fire — overwritten by cb_tvScene within 2.5s |
| cb_loadCalendar | ✓ | ✓ | May fire at midnight — overwritten. By design. |
| cb_sayTime | ✓ | ✓ | Runs normally — TTS unblocked |
| cb_playFragment | ✓ | ✓ | Theme box locked to 144, 2-5s gaps |
| cb_shiftTimer | ✓ | ✓ | Computes shifts — overwritten by next scene |
| WiFi + Web | ✓ | ✓ | Unchanged |
| SensorController | ✓ | ✓ | Runs — harmless |
| AlertRGB | ✓ | ✓ | May flash, then `reapplyCurrentShow()` — overwritten by cb_tvScene |

---

## Edge Cases

| Scenario | Behavior |
|----------|----------|
| Dir /144/ empty or missing | LED flicker works, no audio |
| Calendar midnight rollover | Runs normally — cb_tvScene overwrites within 2.5s. By design. |
| User manually selects pattern | Applied briefly, overwritten by next cb_tvScene |
| AlertRGB during TV mode | Flashes, then reapplyCurrentShow(), overwritten by cb_tvScene |
| OTA update during TV mode | Works normally |
| Power loss during TV mode | RAM cleared → normal boot |
| Duration expires | cb_tvTimeout → exitTvMode() → seamless restore |
| Enter when already active | Route returns `{"active":true}`, no-op |
| cb_tvScene fires after exitTvMode | Can't — `timers.cancel(cb_tvScene)` in exitTvMode |

---

## Open Questions

All resolved.

| # | Question | Answer |
|---|----------|--------|
| 1 | Duration | Slider: 1-12 hours, default 4h |
| 2 | Power-off safety | RAM flag — always safe |
| 3 | LED algorithm | Standard CircleShow with random 2-color params per scene |
| 4 | Guards | None — cb_tvScene overwrites everything |
| 5 | Calendar | Runs unblocked — midnight change is by design |
| 6 | Audio | requestSetSingleDirThemeBox(144), 2-5s gaps |
| 7 | TTS | Runs normally — unblocked |

### Verify during implementation

- [ ] Does `requestSetSingleDirThemeBox()` handle missing dir 144 gracefully?
- [ ] Audio: changing `singleDirMinIntervalMs` mid-flight only affects next schedule (yes — read at schedule time)
