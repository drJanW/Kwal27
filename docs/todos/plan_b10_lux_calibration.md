# Plan B10: Calibrating Lux Sensor (Thumbs-Up Approach)

## Status: Planning
## Date: 2026-02-28

---

## Concept

User sets brightness slider to "looks good", presses 👍 button in web GUI.
Firmware logs (lux, brightness) data point. After 5-10 points in different
light conditions, firmware fits `luxMax`, `luxShiftLo`, `luxShiftHi`, `luxGamma`
from the collected data — all in C++ on the ESP32.

No external tools (no Python, no Excel, no luxmeter).

---

## Current pipeline (for reference)

```
VEML7700.readLux()
  → normalizedLux = clamp(lux, luxMin, luxMax) / luxMax          [0..1]
  → luxT = pow(normalizedLux, luxGamma)                          Stevens' power law
  → luxShift = luxShiftLo + (luxShiftHi - luxShiftLo) × luxT     [-10..+10]%
  → combinedMultiplier = (1+luxShift/100) × (1+calShift/100) × webMult
  → brightness = brightnessHi × combinedMultiplier                clamped [Lo..Hi]
```

**Current defaults (Globals.h):**
| Parameter | Value | Comment |
|-----------|-------|---------|
| luxMin | 0.0 | Sensor floor |
| luxMax | 800.0 | Sensor ceiling — **way too high; expect ~100-200 lux indoor, sensor hidden in jellyfish** |
| luxShiftLo | -10 | Shift at darkest → -10% brightness |
| luxShiftHi | +10 | Shift at brightest → +10% brightness |
| luxGamma | 0.4 | Stevens' exponent (compresses high lux, expands low lux) |
| luxMeasurementDelayMs | 200 | LED blackout before read — **may be too short** |
| luxMeasurementIntervalMs | 2 min | How often to sample ambient |

**Known issues:**
---

## Phase 1: Web GUI — 👍 Button

**Goal:** Add a thumb button next to the brightness slider. On press: trigger
fresh lux measurement, capture data point.

### 1a. Add 👍 button in HTML/JS
- Left of brightness slider row, small icon button (👍 emoji or similar)
- On click: `POST /api/lux/sample` (no body needed)
- Response: `{"ok":true, "lux":42.3, "brightness":180, "n":3}` (n = sample count so far)
- Show brief toast/flash confirming the sample was taken, e.g. "Sample 3: 42 lux → bri 180"

### 1b. Add `/api/lux/sample` endpoint
- **Web handler does NO heavy work** — sets a flag `luxCalibrateSampleRequested = true`
- Returns current sample count + last captured values from memory
- Actual sample capture happens in a `cb_*` timer callback (hard stop 6)

**Files touched:**
- `sdroot/webgui-src/js/` — button + fetch call (new or in existing module)
- `sdroot/index.html` — button element
- `sdroot/styles.css` — minimal button styling
- `lib/WebInterfaceController/routes/` — new route

---

## Phase 2: Firmware — Sample Capture

**Goal:** On 👍 press, take a fresh lux reading and store (lux, brightness) point.

### 2a. Capture logic (in LightRun or new LuxCalibration module)
```
When luxCalibrateSampleRequested:
  1. Trigger lux measurement cycle (same as periodic: fade out → read → fade in)
  2. After read completes, capture data point:
     - lux          = SensorController::ambientLux()   (fresh reading)
     - brightness   = getBrightnessShiftedHi()         (what user approved)
     - sliderPct    = getSliderPct()                   (for context)
     - calendarShift = current calendar shift           (to factor out in fit)
     - patternId    = PatternCatalog::instance().activeId()  (context, NOT fitted)
     - colorsId     = ColorsCatalog::instance().getActiveColorId() (context, NOT fitted)
     - webMultiplier = getWebMultiplier()               (needed to reconstruct intent)
  3. Append to SD file + keep in RAM cache
  4. Log: "[LuxCal] #%d lux=%.1f bri=%u slider=%d cal=%d pat=%s colors=%s"
  5. Clear flag
```

**Why log pattern/color?** They affect perceived brightness. A dark pattern at
bri=200 *looks* different from a bright pattern at bri=200. These fields are
NOT parameters to fit — they're context so the user can later judge which
samples to trust and which to discard. The fit uses only (lux, brightness).

### 2b. Data storage — SD-backed CSV + RAM ring buffer

Calibration runs over **weeks/months** → samples must survive reboots.

**SD file:** `/luxcal.csv` on SD root
```csv
lux;brightness;sliderPct;calShift;webMult;patternId;colorsId;timestamp
42.3;180;65;-2;1.05;rainbow;sunset;2026-03-15T14:30
```
- Max 500 lines (~40KB)
- Simple CSV, easy to inspect/edit manually

**Seeding:** On first boot (no `/luxcal.csv` exists), generate **500 synthetic
samples** from the current model defaults. The FIFO starts completely full —
every slot occupied by a seed. Seeds are ordered **high lux → low lux**,
so the least-interesting high-lux seeds sit at the front and are
overwritten first as real 👍 samples arrive.

```
for i in 0..499:
    lux = luxMax × (499 - i) / 499  // luxMax → 0.0, descending
    brightness = calcShiftedHi(lux, 0, 1.0)  // current model prediction
    write to CSV with patternId="SEED", colorsId="SEED"
```

**Rationale for high→low order:**
- The sensor sits hidden inside the jellyfish, indoors → expect **max ~100-200 lux**
- High-lux seeds (200+ lux) are **unrealistic** in practice → least valuable → replaced first
- Low-lux seeds (~0-50 lux) match the actual operating range → survive longest
- This means early real data (which will mostly be low/mid lux) pushes out
  the irrelevant high-lux seeds, while the useful low-lux seeds stay as
  stabilizers until enough real low-lux data accumulates

**Why 500 seeds?**
- Real-world thumbs-up data is **noisy** (subjective, varying context)
- Until enough real samples accumulate, the fit stays locked to the
  stable model defaults — no wild parameter swings from early outliers
- Each 👍 replaces one seed via FIFO → gradual, smooth transition
- After ~250 real samples the fit is ~50/50 real vs seed — already adapting
- After ~500 real samples all seeds are gone — fully data-driven

**FIFO mechanics:** When a new sample arrives (file already has 500 entries):
- Drop the oldest entry (line 1 after header — initially a high-lux seed)
- Append the new sample at the end
- Rewrite the file (~40KB, ~100ms — fine in timer callback)

This means:
- Collection **never stops** — no hard cap
- Old data naturally ages out
- High-lux seeds (unrealistic range) are replaced first
- Low-lux seeds (actual operating range) survive longest
- The fit smoothly transitions from model-based to data-driven

**Why 500?** Enough for base fit (~20) + per-colors corrections (42×3=126)
+ per-pattern corrections (30×3=90) + margin for duplicates and variety.

**RAM cache:** loaded at boot, mirrors SD file exactly
```cpp
struct LuxCalSample {
    float    lux;           // raw VEML7700 reading
    float    luxBrightness; // brightness normalized to lux-only contribution:
                            //   actualBrightness / ((1+calShift/100) * webMult)
    uint8_t  patternId;     // numeric ID (1-99) for v2 pattern correction
    uint8_t  colorsId;      // numeric ID (1-99) for v2 colors correction
};
constexpr uint16_t MAX_LUX_CAL_SAMPLES = 500;
```
- **10 bytes per sample** × 500 = **5000 bytes** (≈5KB RAM — trivial)
- Non-lux factors (slider, calendarShift, webMultiplier) are divided out at
  capture time, NOT stored. This keeps samples pure lux-vs-brightness.
- patternId / colorsId are numeric IDs matching the CSV `light_pattern_id` /
  `light_colors_id` columns. Both catalogs use `std::vector` internally —
  **no hard limit on number of colors's or patterns** (can grow via CSV).

**Correction arrays (v2):**
```cpp
constexpr uint8_t MAX_CORRECTION_SLOTS = 99;
float colorsCorrection[MAX_CORRECTION_SLOTS] = { 1.0f };  // 396 bytes
float patternCorrection[MAX_CORRECTION_SLOTS] = { 1.0f };  // 396 bytes
```
- All slots initialized to `1.0f` (no correction).
- Currently 42 colors's and 30 patterns, but 99 leaves room for growth.
- Total overhead: **792 bytes** — negligible.
- colorsId/patternId > 99 → no correction applied (silently clamped to 1.0).
- On boot: parse `/luxcal.csv` → fill cache (or seed if absent)
- On 👍: if count < 500 → append; else → drop oldest, append
- SD write via timer callback with `isSdBusy()` guard (hard stop 6)

### 2c. Deduplication / replace
- If new sample has lux within ±10% of existing sample → **replace** that entry (not FIFO drop)
- This prevents clustering around one light condition even within the 100-entry window
- Replacement updates both RAM cache and rewrites SD file
- Seeds (patternId="SEED", colorsId="SEED") are always eligible for replacement by real data

### 2d. Manual curation
- User can edit `/luxcal.csv` on SD (or via download_csv.ps1) to remove bad samples
- E.g. "that sample was taken with a very dark pattern, brightness was set too high"
- After edit: reboot or `/api/lux/reload` to refresh RAM cache

**Files touched:**
- New file: `lib/RunManager/Light/LuxCalibration.cpp` + `.h`
  - RAM cache, SD file read/write, sample capture
- `lib/RunManager/Light/LightRun.cpp` — trigger capture in `cb_*` callback
- `lib/RunManager/Light/LightRun.h` — flag

---

## Phase 3: Firmware — Fit Parameters (C++)

**Goal:** After enough samples (≥4), compute optimal `luxMax`, `luxShiftLo`,
`luxShiftHi`, `luxGamma` using least-squares in C++ on the ESP32.

### 3a. Fitting approach
The model is:
```
brightness_predicted(lux) = brightnessHi × (1 + luxShift(lux) / 100)
where luxShift(lux) = luxShiftLo + (luxShiftHi - luxShiftLo) × pow(lux/luxMax, luxGamma)
```

Simplify: fix `luxMax = max(all sampled lux values) × 1.1` (safe ceiling).
Then fit 3 params: `luxShiftLo`, `luxShiftHi`, `luxGamma`.

### 3b. Fitting algorithm — weighted grid search (simple, robust)
Full nonlinear least squares (Levenberg-Marquardt) is overkill for 3 params and ≤50 points.
Use coarse grid search with **weighted** error — low-lux samples count more:
```
bestError = FLT_MAX
for luxGamma in [0.2, 0.3, 0.4, 0.5, 0.6, 0.7]:
  for luxShiftLo in [-30, -25, -20, -15, -10, -5, 0]:
    for luxShiftHi in [0, +5, +10, +15, +20, +25, +30]:
      error = 0
      for each sample:
        weight = 1.0 / (1.0 + sample.lux)   // low lux → high weight
        error += weight × (predicted - actual)²
      if error < bestError:
        bestError = error
        store best params
```
**Why weighted?** The installation is mostly seen in low/moderate ambient light.
Getting brightness right at lux=5 (evening) matters far more than at lux=500
(bright day when LEDs compete with sunlight anyway). The `1/(1+lux)` weight
makes a fit error at lux=5 count ~100× more than the same error at lux=500.

7 × 7 × 7 = 343 iterations × 500 samples = 171500 pow() calls. At ~1µs each on ESP32:
**~170ms total** — unnoticeable.

Optional refinement: second pass with ±2 steps around best, 0.5 step size.

### 3c. Trigger fit
- Automatically after ≥4 samples, or on explicit "Fit" button press
- `/api/lux/fit` endpoint (or auto-fit on each new sample when n≥4)
- Response: `{"luxMax":320, "luxShiftLo":-15, "luxShiftHi":+20, "luxGamma":0.4, "error":42.3, "n":7}`

### 3d. Apply fitted params
- On fit completion: update `Globals::luxMax`, `luxShiftLo`, `luxShiftHi`, `luxGamma` in memory
- Immediately effective (next lux measurement uses new params)
- Log: `[LuxCal] FIT luxMax=%.0f shiftLo=%d shiftHi=%d gamma=%.2f error=%.1f (n=%d)`

### 3e. Persist to globals.csv (optional, manual trigger)
- `/api/lux/save` — writes fitted params to globals.csv on SD
- Or: just log the params and user updates globals.csv manually
- **Avoids accidental overwrite** — user decides when to commit

### 3f. Per-colors correction factor (v2, after base fit is stable)
Pattern and colors affect perceived brightness. After enough data, compute a
per-colors empirical correction factor — no theory needed, purely data-driven.

**How it works:**
After base fit (lux, brightness) is computed, group samples by colorsId.
For samples at overlapping lux ranges, compare per-colors brightness to mean:
```
For each colorsId with ≥3 samples:
    predicted = base_model(sample.lux)         // from fitted curve
    residuals = sample.brightness - predicted   // what user actually chose
    colorsCorrection[colorsId] = mean(residuals) / brightnessHi
```
Result: a fraction per colors, e.g. Snow White = -0.24 (needs less), Deep Space = +0.25 (needs more).

**Applied in pipeline:**
```
brightness = calcShiftedHi(lux, calShift, webMult) × (1 + colorsCorrection[activeColors])
```

**Data collection protocol for colors calibration:**
1. Pick one pattern (e.g. "solid") and one lux condition (e.g. evening)
2. Cycle through all colors's, setting slider to "looks good" + 👍 for each
3. Repeat at 2-3 different lux levels for robustness
4. ~42 colors's × 3 lux levels = ~126 samples (fills FIFO, old data rolls out)

**Why this works without CIE theory:**
- The correction captures LED spectrum + jellyfish material + eye sensitivity combined
- No assumptions about RGB weighting needed
- CIE luminance `Y = 0.2126R + 0.7152G + 0.0722B` can serve as initial estimate
  before enough per-colors data exists (precalculated from `light_colors.csv` hex values)

**Storage:** `colorsCorrection[99]` — 99 floats = 396 bytes.
Currently 42 colors's in use; room for growth to 99 without code change. Persisted alongside
fitted params, or recalculated from FIFO on each boot.

**NOT in v1** — requires base fit to be stable first.

### 3g. Per-pattern correction factor (v2, same approach as colors)
Same principle as 3f but grouped by patternId instead of colorsId.

```
For each patternId with ≥3 samples:
    predicted = base_model(sample.lux) × (1 + colorsCorrection[sample.colorsId])
    residuals = sample.brightness - predicted
    patternCorrection[patternId] = mean(residuals) / brightnessHi
```

**Applied in pipeline (after colors correction):**
```
brightness = calcShiftedHi(lux, calShift, webMult)
           × (1 + colorsCorrection[activeColors])
           × (1 + patternCorrection[activePattern])
```

**Order matters:** colors correction is computed first (from base-fit residuals),
pattern correction second (from base+colors residuals). This separates the two
effects cleanly — a dark pattern correction isn't contaminated by a dark colors.

**Data collection protocol for pattern calibration:**
1. Pick one colors (e.g. "Snow White" — most neutral)
2. Cycle through all patterns at one lux level, 👍 each
3. Repeat at 2-3 different lux levels
4. ~30 patterns × 3 lux = ~90 samples

**Storage:** `patternCorrection[99]` — 99 floats = 396 bytes.
Currently 30 patterns in use; room for growth to 99 without code change.

**NOT in v1** — do colors corrections first, then add pattern if needed.

**Files touched:**
- `lib/RunManager/Light/LuxCalibration.cpp` + `.h` (created in Phase 2)
  - `addSample()`, `fitParams()`, `applyParams()`, `loadFromSd()`, `getSampleCount()`
  - Grid search algorithm
- `lib/WebInterfaceController/routes/` — `/api/lux/fit`, `/api/lux/save`, `/api/lux/reload`

---

## Phase 4: Web GUI — Status & Controls

**Goal:** Show calibration progress and results in web GUI.

### 4a. Show sample count next to 👍 button
- Badge or small number: "3/16 samples"
- After fit: show fitted params and error

### 4b. Show current lux in brightness area
- Small read-only text: "☀️ 42 lux" next to the slider
- Updates via SSE (add `lux` field to `pushState()`)

### 4c. Optional: "Clear" button
- `/api/lux/clear` — reset samples array AND delete `/luxcal.csv`
- For starting calibration over

### 4e. Optional: "Download samples" link
- `/api/lux/csv` — serves `/luxcal.csv` as download
- For manual inspection/editing on PC

### 4d. Optional: "Apply" confirmation
- After fit, show proposed values and "Apply" button
- Applies to Globals in memory; separate "Save to SD" if desired

**Files touched:**
- `sdroot/webgui-src/js/` — UI updates
- `sdroot/index.html` — lux display element
- `lib/WebInterfaceController/WebGuiStatus.cpp` — add lux to SSE JSON

---

## Phase 5: Cleanup

### 5a. Remove dead code
- `globals.csv` line 44: `#luxBeta;f;0.005` — remove
- `Globals.cpp` line 277: luxBeta removal comment — remove

### 5b. Remove calibration UI after done (optional)
- 👍 button can stay (useful for recalibration later)
- Or hide behind a "calibrate" toggle

### 5c. Move b10 off todo sidelined list

---

## Execution Order

| Step | Phase | Effort | Dependency |
|------|-------|--------|------------|
| 1 | 1a-1b: 👍 button + endpoint | ~45 min | None |
| 2 | 2a-2c: Sample capture logic | ~30 min | Step 1 |
| 3 | 3a-3d: Grid search fit in C++ | ~1 hour | Step 2 |
| 4 | 4a-4b: Status display | ~30 min | Step 2 |
| 5 | 3e: Persist (optional) | ~15 min | Step 3 |
| 6 | User: sample in 4+ light conditions | ~20 min hands-on | Steps 1-3 deployed |
| 7 | 5a-5c: Cleanup | ~15 min | Step 6 validated |

**Total: ~3 hours** code + ~20 min hands-on calibration

---

## Calibration Protocol (user instructions)

This runs over **weeks or months** — no rush. Samples persist on SD.

1. Open web GUI
2. Change room lighting to **dark** → set slider to "looks good" → press 👍
3. Change to **dim/evening** → adjust slider → press 👍
4. Change to **normal** → adjust → press 👍
5. Change to **bright/daylight** → adjust → press 👍
6. Check fitted values in GUI — do they make sense?
7. Over the following weeks: add more samples at different times of day,
   weather conditions, with different patterns/colors active
8. Periodically check fit quality — is the error decreasing?
9. Download `/luxcal.csv` and eyeball: are there outlier samples taken
   with very dark patterns that skew results? Remove them.
10. Save to globals.csv when satisfied

Minimum: **4 samples** across clearly different light conditions.
Ideal: **20-100 samples** for base fit, **200+** for colors/pattern corrections.

**Tip:** Try to sample with a "neutral" pattern (e.g. solid colors or rainbow)
so brightness judgment is less pattern-dependent. The pattern/colors are logged
so you can filter later.

---

## Risks

| Risk | Mitigation |
|------|------------|
| LED bleed during blackout corrupts lux read | Existing fade-out-then-read cycle handles this |
| Too few samples → bad fit | Require ≥4, warn user; grid search is robust even with few points |
| All samples at similar lux → degenerate fit | Deduplication (2c) + user instructions to vary conditions |
| Grid search resolution too coarse | Two-pass: coarse then fine around best |
| Fit produces extreme values | Clamp outputs: luxShiftLo ∈ [-40,0], luxShiftHi ∈ [0,+40], luxGamma ∈ [0.15, 0.8] |
| Pattern/colors bias distorts fit | Log context, user curates outliers; tip to use neutral patterns |
| SD file grows unbounded | FIFO at 500 entries; oldest dropped automatically |
| SD write during audio playback | `isSdBusy()` guard + defer to timer callback |

---

## Open Questions

1. **HOUT sensor**: HOUT has `lux=0` — is there a VEML7700 physically installed, or only on MARMER?
2. **Auto-apply**: Should fit results apply immediately, or require explicit "Apply" button?
3. **Persist**: Write fitted params to globals.csv automatically, or just log and let user copy?
4. **Pattern weighting**: Should the fit eventually weight samples by pattern "neutrality"? (v2, not v1)
