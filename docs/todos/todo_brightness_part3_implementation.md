# TODO Part 3: Implementation — Per-Colors + Per-Pattern Corrections (v2)

## Status: Planning — depends on Part 2 base fit being stable
## Date: 2026-03-02
## Source: plan_b10_lux_calibration.md (Phases 3f, 3g, 4, 5)

---

## Goal

After the base lux curve (Part 2) is calibrated, add per-colors and
per-pattern brightness correction factors derived from accumulated
thumbs-up data. If PNF/CNF (NORMALISATION.txt, calibration.txt) are
approved, they replace the data-driven approach with deterministic
offline normalisation.

**Dev target: HOUT (189) first** — needs strategy for missing lux sensor
(dummy lux / simulated readings / skip lux path).
**Final target: MARMER (188)** — user deploys.

---

## Prerequisites

- Part 2 base fit must be stable (low error, consistent across lux range)
- At least ~126 samples for colors corrections (42 colors × 3 lux levels)
- At least ~90 additional samples for pattern corrections (30 patterns × 3 lux levels)

---

## Tasks

### 3.1 Per-colors correction factor
- [ ] After base fit, group samples by `colorsId`
- [ ] For each colorsId with ≥3 non-clamped, non-seed samples:
  ```
  predicted = base_model(sample.lux)
  residual = sample.luxBrightness - predicted
  colorsCorrection[colorsId] = mean(residuals) / brightnessHi
  ```
- [ ] Result: a fraction per colors (e.g., Snow White = -0.24, Deep Space = +0.25)
- [ ] Store in `colorsCorrection[99]` array (396 bytes)
- [ ] Initialize all slots to 1.0f (no correction)
- [ ] Apply in pipeline:
  ```
  brightness = calcShiftedHi(lux, calShift, webMult) × (1 + colorsCorrection[activeColors])
  ```

### 3.2 Per-pattern correction factor
- [ ] Computed AFTER colors corrections are applied (order matters — see B10 3g)
- [ ] For each patternId with ≥3 non-clamped, non-seed samples:
  ```
  predicted = base_model(sample.lux) × (1 + colorsCorrection[sample.colorsId])
  residual = sample.luxBrightness - predicted
  patternCorrection[patternId] = mean(residuals) / brightnessHi
  ```
- [ ] Store in `patternCorrection[99]` array (396 bytes)
- [ ] Apply in pipeline AFTER colors correction:
  ```
  brightness = calcShiftedHi(...) × (1 + colorsCorrection[c]) × (1 + patternCorrection[p])
  ```

### 3.3 Data collection protocol for colors calibration
- [ ] Document for user: pick one neutral pattern (e.g., Stationary Split #25)
- [ ] Cycle through all 42 colors's at one lux level → 👍 each
- [ ] Repeat at 2-3 different lux levels → ~126 samples
- [ ] Can be done over multiple sessions (samples persist on SD)

### 3.4 Data collection protocol for pattern calibration
- [ ] Document for user: pick one neutral colors (e.g., Snow White #10)
- [ ] Cycle through all 30 patterns at one lux level → 👍 each
- [ ] Repeat at 2-3 different lux levels → ~90 samples

### 3.5 Web GUI — Status display (B10 Phase 4)
- [ ] Show sample count badge next to 👍 button ("n/500")
- [ ] Show current lux reading: "☀️ 42 lux" near brightness slider
- [ ] Add `lux` field to SSE `pushState()` in WebGuiStatus.cpp
- [ ] After fit: show fitted params + error in GUI
- [ ] Optional: "Apply" confirmation before committing fit results
- [ ] Optional: "Clear" button to restart calibration
- [ ] Optional: "Download CSV" link for manual inspection

### 3.6 Persist correction arrays
- [ ] Save colorsCorrection[] and patternCorrection[] to SD
  (either in globals.csv or a separate corrections.csv)
- [ ] Reload on boot
- [ ] OR: recalculate from luxcal.csv on every boot (~170ms, trivial)

### 3.7 Cleanup (B10 Phase 5)
- [ ] Remove `#luxBeta;f;0.005` from globals.csv line 44
- [ ] Remove luxBeta comment from Globals.cpp line 277
- [ ] Move B10 off todo sidelined list
- [ ] Optionally hide calibration UI behind a toggle after done

---

## Correction slot limits

| Resource | Current | Max slots | RAM |
|----------|---------|-----------|-----|
| colors's | 42 | 99 | 396 bytes |
| patterns | 30 | 99 | 396 bytes |
| Total | 72 | 198 | 792 bytes |

IDs > 99 get no correction (1.0f). Acceptable for foreseeable growth.
If CSV ever exceeds 99 entries, increase `MAX_CORRECTION_SLOTS` and resize arrays.

---

## Runtime formula (final, with all corrections)

```
normalizedLux = clamp(lux, luxMin, luxMax) / luxMax
luxT = pow(normalizedLux, luxGamma)
luxShift = luxShiftLo + (luxShiftHi - luxShiftLo) × luxT

combinedMultiplier = (1 + luxShift/100)
                   × (1 + calendarShift/100)
                   × webMultiplier
                   × (1 + colorsCorrection[activeColors])   // Part 3 addition
                   × (1 + patternCorrection[activePattern])  // Part 3 addition

brightness = brightnessHi × combinedMultiplier
           → clamped to [brightnessLo, brightnessHi]
```

---

## Files touched (cumulative across all 3 parts)

| File | What |
|------|------|
| `lib/RunManager/Light/LuxCalibration.h` + `.cpp` | **NEW** — RAM cache, SD I/O, fit, corrections |
| `lib/RunManager/Light/LightPolicy.cpp` | Add colors + pattern correction multipliers |
| `lib/RunManager/Light/LightRun.cpp` + `.h` | Flag, trigger capture in cb_* |
| `lib/WebInterfaceController/routes/` | New lux routes (/sample, /fit, /save, /clear, /reload, /csv) |
| `lib/WebInterfaceController/WebGuiStatus.cpp` | Add lux + sample count to SSE |
| `sdroot/index.html` | 👍 button, lux display, status area |
| `sdroot/styles.css` | Button + badge styling |
| `sdroot/webgui-src/js/` | JS for button, toast, status updates |
| `sdroot/globals.csv` | Remove luxBeta, update comments |
| `lib/Globals/Globals.cpp` | Remove luxBeta comment |

---

## Open Issues

| # | Issue | Status |
|---|-------|--------|
| Q4 | Stick with shift model or switch to a*(lux+b)^c? | Depends on E1 |
| Q8 | 99 correction slots enough? | Yes for now |
| E2 | NORMALISATION.txt + calibration.txt had MAX_AUDIO_VOLUME | FIXED 2026-03-02 |
| E1 | Model mismatch between docs | OPEN — discuss next |
