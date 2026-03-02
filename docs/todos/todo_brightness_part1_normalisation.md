# TODO Part 1: Brightness Normalisation (PNF + CNF)

## Status: Planning — PNF/CNF concept awaiting approval
## Date: 2026-03-02
## Source: BRIGHTNESS NORMALISATION.txt, calibration.txt

---

## Goal

Ensure all pattern + colors combinations produce comparable perceived brightness
at identical settings. This removes pattern/color variability so the lux
calibration (Part 2) only has to deal with ambient light.

**Dev target: HOUT (189)** — normalisation code + web GUI developed here.

---

## Decision status

> **Q1/Q2 ANSWERED**: PNF and CNF are fresh, untested ideas from
> BRIGHTNESS NORMALISATION.txt and calibration.txt. If the concept is
> approved after review, it overrules plan_b10_lux_calibration.md.
> Until then, both approaches remain on the table.

**Option A — NORMALISATION.txt approach (offline, Excel-based)**
- Compute PNF per pattern and CNF per colors in Excel
- Store as fixed lookup tables on SD
- Freeze before starting lux calibration
- Pro: deterministic, reproducible
- Con: requires test rig, Excel workflow, manual measurement

**Option B — B10 approach (on-device, from thumbs-up data)**
- Skip separate PNF/CNF computation
- Do base lux calibration first (Part 2)
- Then derive per-colors and per-pattern corrections from residuals
- Pro: no external tools, self-calibrating
- Con: corrections are subjective, need many samples, order-dependent

**Option C — Hybrid**
- Compute CIE luminance from `light_colors.csv` hex values as initial CNF estimate
  (purely mathematical, no measurement needed)
- Skip PNF (too subjective for offline calculation)
- Refine both via thumbs-up data over time (B10 v2)
- Pro: instant initial correction for colors + data-driven refinement
- Con: CIE luminance doesn't account for jellyfish material/LED spectrum

---

## Tasks (assuming Option C or B — no Excel workflow)

### 1.1 Compute initial CNF from light_colors.csv hex values
- [ ] For each colors entry: parse `rgb1_hex`, `rgb2_hex`
- [ ] Compute CIE luminance: `Y = 0.2126*R + 0.7152*G + 0.0722*B` (both colors, average)
- [ ] Normalize to reference (Snow White = 1.0): `CNF = Y_white / Y_colors`
- [ ] Store as `colorsCorrection[]` array at boot
- [ ] This is a STATIC correction — computed once, no calibration needed

### 1.2 Decide where PNF comes from
- [ ] Option: skip PNF entirely in v1 (patterns are too complex to model without measurement)
- [ ] Option: use `calculate_unscaled_power_mW()` from FastLED to estimate per-pattern power
  BUT: this only works at runtime with a running pattern, not offline
- [ ] Decision: defer PNF to Part 2 (data-driven from thumbs-up residuals)

### 1.3 Integrate CNF into brightness pipeline
- [ ] Modify `calcShiftedHi()` in LightPolicy.cpp:
  ```
  brightness = brightnessHi × combinedMultiplier × colorsCorrection[activeColors]
  ```
- [ ] Or: multiply into `combinedMultiplier` before clamping
- [ ] Ensure clamp still applies AFTER correction

### 1.4 Resolve maxBrightness vs brightnessHi (E9)
- [ ] Clarify: `maxBrightness` = hardware cap, `brightnessHi` = operational ceiling?
- [ ] Document the distinction or merge if redundant
- [ ] Ensure normalisation multiplies against the right one

### 1.5 Clean up dead globals
- [ ] Remove `#luxBeta;f;0.005` from globals.csv line 44
- [ ] Remove corresponding comment in Globals.cpp line 277

---

## Dependencies

- Must be done BEFORE Part 2 (lux calibration) starts collecting real data
  (NORMALISATION.txt rule 3: "LuxScale is calibrated after PNF/CNF frozen")
- OR: skip this step entirely and accept that Part 2 data includes
  colors/pattern bias (plan_b10's approach — correct after the fact)

---

## Open Issues

| # | Issue | Status |
|---|-------|--------|
| E9 | maxBrightness vs brightnessHi ambiguity | Needs decision |
| E10 | PNF/CNF ordering vs plan_b10's skip-and-correct-later | Depends on Q1/Q2 approval |
| Q2 | Separate PNF/CNF or data-driven corrections? | PNF/CNF awaiting approval |
