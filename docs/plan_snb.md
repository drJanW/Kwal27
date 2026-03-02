# SNB — Stevens Normalized Brightness

## One-liner
Lux-adaptive LED brightness using Stevens' power law, with PNF and CNF
normalisation so every pattern + colors combination looks equally bright.

## Status: Approved (lux curve model) / PNF+CNF awaiting concept approval
## Date: 2026-03-02

---

## What SNB is

Three layers, applied in order:

1. **PNF** — Pattern Normalisation Factor (compensates brightness differences
   between patterns)
2. **CNF** — Colors Normalisation Factor (compensates luminance differences
   between colors's)
3. **Lux curve** — Stevens' power law maps ambient lux to a brightness shift

All three are multiplicative. PNF and CNF are frozen first, then the lux
curve is calibrated on top.

---

## The model

### Lux curve (implemented, in LightPolicy.cpp)

```
normalizedLux = clamp(lux, luxMin, luxMax) / luxMax           [0..1]
luxT          = pow(normalizedLux, luxGamma)                  Stevens' power law
luxShift      = luxShiftLo + (luxShiftHi - luxShiftLo) × luxT  percentage shift
```

### Runtime formula (current)

```
combinedMultiplier = (1 + luxShift / 100)
                   × (1 + calendarShift / 100)
                   × webMultiplier

brightness = brightnessHi × combinedMultiplier
           → clamped to [brightnessLo, brightnessHi]
```

### Runtime formula (target, after PNF+CNF approval)

```
combinedMultiplier = (1 + luxShift / 100)
                   × (1 + calendarShift / 100)
                   × webMultiplier
                   × PNF[activePattern]
                   × CNF[activeColors]

brightness = brightnessHi × combinedMultiplier
           → clamped to [brightnessLo, brightnessHi]
```

### Parameters

| Parameter | Default | Range | What it does |
|-----------|---------|-------|-------------|
| luxMin | 0.0 | ≥0 | Sensor floor (clamped) |
| luxMax | 800.0 | >0 | Sensor ceiling — **should be ~100-200 for indoor jellyfish** |
| luxShiftLo | -10 | [-40..0] | Brightness shift at darkest (lux=0) |
| luxShiftHi | +10 | [0..+40] | Brightness shift at brightest (lux=luxMax) |
| luxGamma | 0.4 | [0.15..0.8] | Stevens' exponent: <1 compresses high lux, expands low lux |

### Why Stevens' power law

The jellyfish is indoors; real lux range is ~0–100. Getting brightness right
at lux=5 (evening) matters far more than at lux=500 (bright day, LEDs already
at max). The gamma exponent < 1 allocates most of the shift range to low lux:

| lux | normalizedLux (luxMax=100) | luxT (γ=0.4) | % of shift range used |
|-----|---------------------------|---------------|----------------------|
| 5   | 0.05                      | 0.26          | 26% |
| 20  | 0.20                      | 0.53          | 53% |
| 50  | 0.50                      | 0.76          | 76% |
| 100 | 1.00                      | 1.00          | 100% |

Half the shift range is allocated to lux 0–20. This matches the installation's
actual operating conditions.

---

## PNF — Pattern Normalisation Factor

**Status: concept, awaiting approval**

Compensates for intrinsic brightness differences between patterns.
A "Twinkling Stars" pattern at bri=200 looks dimmer than "Stationary Split"
at bri=200 because fewer LEDs are lit at any given moment.

```
PNF(pattern) = TargetPower / MeanPatternPower
```

Measurement: run each pattern for 5 minutes with white colors at fixed
brightness, accumulate `calculate_unscaled_power_mW()`, compute mean.
Store as lookup table on SD.

---

## CNF — Colors Normalisation Factor

**Status: concept, awaiting approval**

Compensates for luminance differences between colors's.
Snow White (#FFFFFF) at bri=200 looks far brighter than Deep Space (#000000)
at bri=200.

```
CNF(colors) = ReferenceColorPower / MeanColorPower
```

Can be bootstrapped from CIE luminance of the hex values:
`Y = 0.2126×R + 0.7152×G + 0.0722×B` (both colors of the pair, averaged).
Refined later via thumbs-up data.

---

## Calibration method

**Thumbs-up approach** (from plan_b10_lux_calibration.md):

1. User sees brightness slider in web GUI
2. Adjusts until it "looks good"
3. Presses 👍 button → firmware logs (lux, brightness) data point
4. After ≥4 points at different lux levels: grid search fits
   `luxShiftLo`, `luxShiftHi`, `luxGamma` (luxMax fixed from data)
5. Weighted fit: `weight = 1/(1+lux)` — low-lux samples count more

Calibration runs on **MARMER (188)** only (has lux sensor).
Development on **HOUT (189)** (normalisation code, web GUI).

---

## Execution plan (3 parts)

| Part | What | Where | Doc |
|------|------|-------|-----|
| 1 — Normalisation | PNF + CNF computation and integration | HOUT | todo_brightness_part1_normalisation.md |
| 2 — Calibration | 👍 button, data collection, grid search fit | MARMER | todo_brightness_part2_calibration.md |
| 3 — Implementation | Per-colors + per-pattern corrections (v2), status GUI, cleanup | HOUT → MARMER | todo_brightness_part3_implementation.md |

---

## Ordering rule

1. Freeze PNF and CNF first
2. Then calibrate lux curve
3. If PNF or CNF changes → redo lux calibration

---

## Source documents (historical)

These documents fed into SNB. Refer to plan_snb.md going forward.

| Document | Role |
|----------|------|
| BRIGHTNESS NORMALISATION.txt | PNF/CNF theory, lux adaptation concept |
| calibration.txt | PNF/CNF measurement procedure, pseudocode |
| plan_b10_lux_calibration.md | Thumbs-up calibration, FIFO, grid search, implementation detail |
| todo_normcalibeq.txt | Error/question tracker across all three |

---

## Invariants

1. **Lux is ambient-only** — measured when all LEDs are OFF (black state)
2. **PNF and CNF are frozen before lux calibration**
3. **No circular recalibration** — if PNF/CNF change, redo lux curve
4. **Channel shifts / white balance are separate** — never merged into global gain
5. **Fades are applied AFTER the combined multiplier**
