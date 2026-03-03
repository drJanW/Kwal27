# TODO Part 1: Brightness Normalisation (PNF + CNF)

## Status: DONE
## Date: 2026-03-02
## Source: BRIGHTNESS NORMALISATION.txt, calibration.txt, plan_snb.md

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

### ~~1.1 Compute initial CNF from light_colors.csv hex values~~ DONE 2026-03-02
- [x] ColorsCatalog::ensureCnf() computes CIE luminance ratio per colors
- [x] Reference = Snow White (cnf=1.0), computed at CSV load, saved to SD
- [x] Static correction — computed once, no calibration needed

### ~~1.2 PNF stub (all 1.0f, calibrated later)~~ DONE 2026-03-02
- [x] PatternCatalog::ensurePnf() sets 0→1.0f in memory after CSV load
- [x] pnf() accessor returns entry->pnf directly (ensurePnf guarantees non-zero)
- [x] Zero behavioral change until calibrated

### ~~1.3 Integrate CNF + PNF into brightness pipeline~~ DONE 2026-03-02
- [x] calcShiftedHi() multiplies with cnf and pnf
- [x] LightRun and WebInterfaceController pass pnf through
- [x] Clamp applies after correction

### ~~1.4 Resolve maxBrightness vs brightnessHi (E9)~~ DONE 2026-03-02
- [x] maxBrightness = 248 (hardware), brightnessHi = 242 (policy). Distinct values.
- [x] Documented in todo_normcalibeq.txt E9.

### ~~1.5 Clean up dead globals~~ DONE 2026-03-02
- [x] luxBeta already removed from globals.csv and Globals.cpp (prior session)

---

## Dependencies

- Must be done BEFORE Part 2 (lux calibration) starts collecting real data.
  SNB ordering rule: "Freeze PNF and CNF first → then calibrate lux curve."
  See plan_snb.md.

---

## Resolved Issues

| # | Issue | Resolution |
|---|-------|------------|
| E9 | maxBrightness vs brightnessHi | NOT AN ERROR — distinct concepts, values now differ (248 vs 242) |
| E10 | PNF/CNF ordering vs plan_b10 | RESOLVED — SNB plan overrules: Part 1 before Part 2 |
| Q2 | Separate PNF/CNF or data-driven? | ANSWERED — PNF/CNF awaiting approval; if approved, mandatory |
