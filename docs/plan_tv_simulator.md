# Plan: TV Simulator Mode

## Concept

One-button WebGUI toggle that makes the Kwal look like a TV is on — flickering blue-white light + TV ambience audio. All normal scheduling (shifts, saytime, calendar, rotation, audio) pauses while active.

---

## Components

### 1. CSV Data (no code)

**`light_colors.csv`** — add row:
```
42;TV Screen;#C0D8FF;#4477BB;0
```
Cool blue-white pair mimicking TV screen glow.

**`light_patterns.csv`** — add row:
```
29;TV Simulator;1;1;99.000;1;1.500;0.000;0.000;40.000;48;30.000;20.000;15.000;3;7;0
```
Key values:
- `colorCycleSec=1`, `brightCycleSec=1` → maximum speed
- `radiusOsc=30` → heavy pulsing
- `xCycleSec=3`, `yCycleSec=7` → prime numbers, so the combined motion repeats only every 21 seconds → visually unpredictable
- `fadeWidth=99` → diffuse, no sharp ring
- `minBrightness=1` → deep dark moments between flickers

**SD card** — dir `/150/` with 10-30 royalty-free TV ambience MP3's (`001.mp3`–`030.mp3`): news mumble, commercials, zap sounds, game show jingles. Same bitrate as existing MP3's.

---

### 2. Global Flag

**`Globals.h`**:
```cpp
inline static bool tvMode = false;
```

---

### 3. Guards in Callbacks (~6 lines, 4 files)

Each guarded callback gets one line at the top:

**`LightRun.cpp`**:
```cpp
void LightRun::cb_shiftTimer() {
    if (Globals::tvMode) return;        // ← ADD
    // ... existing code ...
}

void LightRun::cb_changeColor() {
    if (Globals::tvMode) return;        // ← ADD
    // ... existing code ...
}

void LightRun::cb_changePattern() {
    if (Globals::tvMode) return;        // ← ADD
    // ... existing code ...
}
```

**`CalendarRun.cpp`**:
```cpp
void CalendarRun::cb_loadCalendar() {
    if (Globals::tvMode) return;        // ← ADD
    // ... existing code ...
}
```

**`RunManager.cpp`**:
```cpp
void cb_sayTime() {
    if (Globals::tvMode) return;        // ← ADD
    // ... existing code (incl. self-reschedule) ...
}

void cb_playFragment() {
    if (Globals::tvMode) return;        // ← ADD
    // ... existing code (incl. self-reschedule) ...
}
```

Timers stay running — they just no-op. When tvMode turns off, next fire resumes normal behavior. No cancel/restart needed.

---

### 4. Endpoint (~10 lines C++)

**`WebInterfaceController/routes/` — new or existing routes file**:

```
GET /api/tvmode?active=1   → enable
GET /api/tvmode?active=0   → disable
```

Pseudo-code:
```cpp
void routeTvMode(AsyncWebServerRequest *request) {
    bool active = request->hasParam("active")
                  && request->getParam("active")->value() == "1";

    Globals::tvMode = active;

    if (active) {
        // 1. Select TV pattern + colors by ID
        LightRun::applyPattern(29);          // "TV Simulator"
        LightRun::applyColor(42);            // "TV Screen"

        // 2. Force max brightness
        setWebMultiplier(???);               // Check: what value = max?

        // 3. Start TV audio from dir 150
        RunManager::requestSetSingleDirThemeBox(150);
        RunManager::requestPlaySpecificFragment(150, -1, "tvmode");
    } else {
        // 1. Restore normal brightness
        setWebMultiplier(1.0f);

        // 2. Release theme box lock
        // (existing expiry mechanism or explicit clear)

        // 3. Set sources back to CONTEXT so rotation/calendar can resume
        // patternSource = LightSource::CONTEXT;
        // colorSource   = LightSource::CONTEXT;

        // 4. Immediately apply current context (calendar/default)
        // triggers on next cb_loadCalendar or cb_changeColor/Pattern fire
    }

    request->send(200, "text/plain", active ? "TV ON" : "TV OFF");
}
```

Open questions marked with `???`:
- What `setWebMultiplier()` value gives absolute max brightness? Need to check current brightness pipeline.
- How to cleanly release the single-dir theme box? Check if `clearThemeBox()` or similar exists.
- Should audio loop continuously? Currently `cb_playFragment` is paused — need a TV-specific audio loop timer or rely on the single-dir scheduling.

---

### 5. WebGUI Button (~3 lines HTML, ~10 lines JS)

**`sdroot/index.html`** — add button in appropriate section:
```html
<button id="btnTvMode" class="tv-btn" onclick="toggleTvMode()">📺 TV</button>
```

**`sdroot/webgui-src/js/` — appropriate JS module**:
```javascript
let tvModeActive = false;

function toggleTvMode() {
    tvModeActive = !tvModeActive;
    fetch(`/api/tvmode?active=${tvModeActive ? 1 : 0}`)
        .then(r => r.text())
        .then(txt => {
            document.getElementById('btnTvMode').classList.toggle('active', tvModeActive);
        });
}
```

**`sdroot/styles.css`** — button styling:
```css
.tv-btn        { /* normal state */ }
.tv-btn.active { /* glowing/highlighted when TV mode is on */ }
```

---

## Audio During TV Mode

When TV mode activates:
1. Set theme box to dir 150 (locks audio to TV sounds)
2. Play first fragment immediately
3. **Problem**: `cb_playFragment` is guarded → won't auto-schedule next fragment

**Solutions** (pick one):
- **A)** Don't guard `cb_playFragment`, instead let the theme box restriction handle it — fragments play from dir 150 only. Downside: normal scheduling intervals (6-48 min) are too long.
- **B)** Add a separate `cb_tvAudioLoop` timer (every 30-90 sec) that only fires when `tvMode == true`, plays next random from dir 150. Cancel when TV mode off.
- **C)** Shorten fragment interval via `requestSetAudioIntervals()` when TV mode activates (existing API). Restore on deactivate.

**Recommended: C** — uses existing mechanism, no new timer needed.

---

## Activation Flow

```
User taps 📺 TV button
  → JS: fetch /api/tvmode?active=1
    → ESP: Globals::tvMode = true
    → ESP: applyPattern(29) + applyColor(42)
    → ESP: setWebMultiplier(max)
    → ESP: setSingleDirThemeBox(150)
    → ESP: setAudioIntervals(short)
    → ESP: playFragment(150, random)
  → JS: button gets .active class

All other callbacks see tvMode==true → return early → no interference

User taps 📺 TV button again
  → JS: fetch /api/tvmode?active=0
    → ESP: Globals::tvMode = false
    → ESP: setWebMultiplier(1.0)
    → ESP: clear theme box
    → ESP: restore audio intervals
    → ESP: sources → CONTEXT
  → JS: button loses .active class

Next timer fires of cb_changeColor/Pattern/etc → normal operation resumes
```

---

## File Impact Summary

| File | Change | Lines |
|------|--------|-------|
| `light_colors.csv` | +1 row (TV Screen) | 1 |
| `light_patterns.csv` | +1 row (TV Simulator) | 1 |
| `Globals.h` | +1 bool `tvMode` | 1 |
| `LightRun.cpp` | +3 guards | 3 |
| `CalendarRun.cpp` | +1 guard | 1 |
| `RunManager.cpp` | +2 guards | 2 |
| New routes file or existing | +1 endpoint | ~15 |
| `index.html` | +1 button | ~3 |
| `webgui-src/js/*.js` | +1 toggle function | ~10 |
| `styles.css` | +button style | ~5 |
| **Total** | | **~42 lines** |

---

## Open Questions

1. What `setWebMultiplier()` value = absolute max brightness?
2. How to clear single-dir theme box on deactivate?
3. Audio solution A, B, or C?
4. Should TV mode auto-deactivate after N hours (like silence mode's 13h expiry)?
5. SD dir number: 150 confirmed?
6. TV pattern parameters: tune after first test on hardware?
