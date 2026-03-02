# TODO Part 2: Lux Calibration (Data Collection + Curve Fitting)

## Status: Planning — blocked on E1/E3-E7 decisions
## Date: 2026-03-02
## Source: plan_b10_lux_calibration.md, calibration.txt

---

## Goal

Calibrate the 4 lux-related parameters (`luxMax`, `luxShiftLo`, `luxShiftHi`,
`luxGamma`) from real user data so the jellyfish looks right at every ambient
light level. Uses the existing Stevens' power law pipeline.

**Calibration target: MARMER (188)** — only device with lux sensor.
User deploys to MARMER; Copilot never uploads to 188.

---

## Errors to resolve before coding

### E3: luxMeasurementDelayMs mismatch
- Globals.h default = 800ms, globals.csv = 200ms, B10 table says 200ms
- **Action**: decide correct value. 800ms is safer (VEML7700 integration time).
  200ms risks reading residual LED light.

### E5: Clamping breaks luxBrightness normalization
- `calcShiftedHi()` clamps to [brightnessLo, brightnessHi] before returning.
- If we capture the clamped value and divide out calShift×webMult, we get a
  wrong number whenever clamping occurred.
- **Fix options**:
  - (a) Capture pre-clamp value (add a version of calcShiftedHi that returns float)
  - (b) Capture `combinedMultiplier` instead of brightness (then fit against multiplier, not brightness)
  - (c) Store raw slider position + lux and reconstruct at fit time
  - (d) Detect clamping: if result == brightnessLo or brightnessHi, mark sample as clamped → exclude from fit
- **Recommended**: (d) — simplest, minimal code change

### E6: SEED marker vs uint8_t
- CSV uses `patternId="SEED"`, RAM uses uint8_t
- **Fix**: use `patternId=0` and `colorsId=0` as seed sentinel in RAM.
  0 is never a valid CSV id (IDs start at 1).

### E7: Dedup vs FIFO conflict
- **Fix options**:
  - (a) FIFO only — accept clustering risk
  - (b) Dedup only — replace closest match, never grow beyond 500
  - (c) Priority: check dedup first; if match found → replace; else FIFO drop oldest + append
- **Recommended**: (c) — dedup takes priority, FIFO is fallback

### E8: IDs are String, not uint8_t
- `PatternCatalog::activeId()` returns String (e.g., "15")
- **Fix**: `.toInt()` with bounds check (1-99 → uint8_t, else 0 = unknown)

---

## Tasks

### 2.1 Web GUI — 👍 button (B10 Phase 1)
- [ ] Add 👍 button next to brightness slider in `sdroot/index.html`
- [ ] Style in `sdroot/styles.css` (small, unobtrusive)
- [ ] JS handler in `sdroot/webgui-src/js/` — `POST /api/lux/sample`
- [ ] Toast/flash showing sample count + captured values
- [ ] Show current sample count as badge on button

### 2.2 Firmware — `/api/lux/sample` endpoint
- [ ] New route in `lib/WebInterfaceController/routes/`
- [ ] Handler sets flag `luxCalSampleRequested = true` (memory only, hard stop 6)
- [ ] Returns `{"ok":true, "lux":..., "brightness":..., "n":...}` from cached values

### 2.3 Firmware — Sample capture logic (LuxCalibration module)
- [ ] New files: `lib/RunManager/Light/LuxCalibration.h` + `.cpp`
- [ ] RAM struct:
  ```cpp
  struct LuxCalSample {
      float    lux;            // raw VEML7700 reading
      float    luxBrightness;  // lux-only brightness (calShift+webMult divided out)
      uint8_t  patternId;      // 1-99, 0=SEED
      uint8_t  colorsId;       // 1-99, 0=SEED
      bool     clamped;        // true if brightness hit Lo or Hi bound
  };
  ```
- [ ] 500-entry array in RAM (~6KB)
- [ ] `addSample()` — dedup check first, then FIFO fallback
- [ ] `loadFromSd()` — parse `/luxcal.csv` at boot
- [ ] `saveToSd()` — rewrite CSV (timer callback, isSdBusy guard)
- [ ] Seed generation if no CSV exists (500 synthetic samples, high→low lux)

### 2.4 Trigger sample in cb_* callback
- [ ] In `LightRun`: when `luxCalSampleRequested` flag is set:
  1. Trigger fresh lux measurement (reuse existing fade-out-read-fade-in cycle)
  2. After read: capture (lux, brightness, context)
  3. Detect clamping: `if (brightness == brightnessLo || brightness == brightnessHi)`
  4. Convert String IDs to uint8_t
  5. Divide out calShift and webMult to get luxBrightness
  6. Call `LuxCalibration::addSample()`
  7. Clear flag

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

## Open Issues

| # | Issue | Status |
|---|-------|--------|
| Q3 | HOUT has no lux sensor — how to test? | ANSWERED: calibration on MARMER only |
| Q5 | Clamping distortion fix approach | Decide (d) recommended |
| Q6 | Dedup vs FIFO priority | Decide (c) recommended |
| Q7 | 500 seeds too many? | Needs discussion |
| E3 | luxMeasurementDelayMs: 200 vs 800? | Needs decision |
