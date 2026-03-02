# TODO Part 2: Lux Calibration (Data Collection + Curve Fitting)

## Status: Planning — no blockers remaining
## Date: 2026-03-02
## Source: plan_b10_lux_calibration.md, calibration.txt, plan_snb.md

---

## Goal

Calibrate the 4 lux-related parameters (`luxMax`, `luxShiftLo`, `luxShiftHi`,
`luxGamma`) from real user data so the jellyfish looks right at every ambient
light level. Uses the existing Stevens' power law pipeline.

**Calibration target: MARMER (188)** — only device with lux sensor.
User deploys to MARMER; Copilot never uploads to 188.

---

## Design notes (from resolved errors)

- **E6 — ID sentinels**: use `patternId=0` / `colorsId=0` as seed sentinel
  (IDs start at 1). No "SEED" strings in RAM.
- **E7/Q6 — Buffer policy**: dedup vs FIFO and overflow strategy TBD when
  designing the data store. See todo_normcalibeq.txt Q6.
- **E8 — String→uint8_t**: `PatternCatalog::activeId()` returns String.
  Use `.toInt()` + bounds check when storing numeric IDs.

---

## Tasks

### 2.1 Web GUI — Calibration panel
- [ ] Add calibration toggle/panel in `sdroot/index.html`
- [ ] When active: show 👍 button next to brightness slider
- [ ] Style in `sdroot/styles.css` (toggleable, unobtrusive)
- [ ] JS handler in `sdroot/webgui-src/js/` — `POST /api/lux/calibrate` (on/off)
- [ ] JS handler for 👍 — `POST /api/lux/sample`
- [ ] Toast/flash showing sample count + captured values
- [ ] Show current sample count as badge on 👍 button

### 2.2 Firmware — Calibration-mode endpoints
- [ ] `/api/lux/calibrate` — sets `calibrationMode = true/false` (memory only, hard stop 6)
- [ ] While `calibrationMode` is true: `calcShiftedHi()` stores pre-clamp float
  in `Globals::lastUnclampedBrightness` (solves E5 clamping problem)
- [ ] `/api/lux/sample` — sets flag `luxCalSampleRequested = true` (memory only)
- [ ] Returns `{"ok":true, "lux":..., "brightness":..., "n":...}` from cached values

### 2.3 Firmware — Sample capture logic (LuxCalibration module)
- [ ] New files: `lib/RunManager/Light/LuxCalibration.h` + `.cpp`
- [ ] RAM struct:
  ```cpp
  struct LuxCalSample {
      float    lux;            // raw VEML7700 reading
      float    luxBrightness;  // unclamped brightness (from calibration-mode)
      uint8_t  patternId;      // 1+, 0=seed sentinel (E6)
      uint8_t  colorsId;       // 1+, 0=seed sentinel (E6)
  };
  ```
- [ ] Buffer size TBD (Q6/Q7 deferred — decide during implementation)
- [ ] `addSample()` — buffer policy TBD (dedup vs FIFO, see Q6)
- [ ] `loadFromSd()` — parse `/luxcal.csv` at boot
- [ ] `saveToSd()` — rewrite CSV (timer callback, isSdBusy guard)

### 2.4 Trigger sample in cb_* callback
- [ ] In `LightRun`: when `luxCalSampleRequested` flag is set:
  1. Trigger fresh lux measurement (reuse existing fade-out-read-fade-in cycle)
  2. After read: capture `(lux, lastUnclampedBrightness)` from calibration-mode
  3. Convert String IDs to uint8_t (E8: `.toInt()` + bounds)
  4. Call `LuxCalibration::addSample()`
  5. Clear flag

### 2.5 Grid search fit (B10 Phase 3)
- [ ] `fitParams()` in LuxCalibration.cpp
- [ ] Fix `luxMax = max(all sample lux) × 1.1`
- [ ] Grid search 3 params:
  - `luxShiftLo` ∈ [-30 .. 0] step 5 → 7 values
  - `luxShiftHi` ∈ [0 .. +30] step 5 → 7 values
  - `luxGamma` ∈ [0.2 .. 0.7] step 0.1 → 6 values
- [ ] Weighted error: `weight = 1.0 / (1.0 + sample.lux)`
- [ ] Exclude clamped samples from fit
- [ ] Optional: second pass ±2 steps at 0.5× step size around best
- [ ] Clamp outputs: luxShiftLo ∈ [-40,0], luxShiftHi ∈ [0,+40], luxGamma ∈ [0.15,0.8]
- [ ] Auto-fit when sampleCount ≥ 4 (excluding seeds and clamped)

### 2.6 Apply + persist fitted params
- [ ] On fit: update `Globals::luxMax`, `luxShiftLo`, `luxShiftHi`, `luxGamma` in memory
- [ ] `/api/lux/fit` endpoint — trigger fit manually, return results
- [ ] `/api/lux/save` endpoint — write fitted params to globals.csv (user-triggered)
- [ ] Log: `[LuxCal] FIT luxMax=%.0f shiftLo=%d shiftHi=%d gamma=%.2f error=%.1f (n=%d)`

### 2.7 Support endpoints
- [ ] `/api/lux/clear` — reset samples + delete CSV
- [ ] `/api/lux/reload` — re-read CSV into RAM (after manual edits)
- [ ] `/api/lux/csv` — serve CSV as download

---

## Calibration protocol (user manual)

1. Open web GUI
2. Dark room → set slider to "looks good" → press 👍
3. Dim/evening → adjust → 👍
4. Normal daylight → adjust → 👍
5. Bright daylight → adjust → 👍
6. Over weeks: add more samples at varying conditions
7. Periodically download CSV and remove outlier samples
8. When happy: press Save to commit to globals.csv

Minimum: **4 real samples** across clearly different lux levels.
Ideal: **20-100 samples** for stable base fit.

---

## Resolved Issues

| # | Issue | Resolution |
|---|-------|------------|
| E1 | Model mismatch | Keep Stevens' power law (SNB). See plan_snb.md |
| E3 | luxMeasurementDelayMs | FIXED — wired as sensor settling delay |
| E5 | Clamping distortion | Solved by calibration-mode (pre-clamp float stored) |
| Q3 | HOUT has no lux sensor | Calibration on MARMER only |
| Q5 | Clamping fix approach | Calibration-mode |
| Q6 | Dedup vs FIFO | Deferred to implementation |
| Q7 | 500 seeds excessive? | Belongs to old approach, buffer size TBD |
