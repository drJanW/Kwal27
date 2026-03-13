# SNB — Sensor Normalized Brightness

## One-liner
Lux-adaptive LED brightness using exponential saturation, with PNF and CNF
normalisation so every pattern + colors combination looks equally bright.

## Status: Implemented (lux curve + PNF + CNF) / Calibration via web GUI
## Date: 2026-03-13

---

## What SNB is

Three layers, applied in order:

1. **PNF** — Pattern Normalisation Factor (compensates brightness differences
   between patterns)
2. **CNF** — Colors Normalisation Factor (compensates luminance differences
   between colors's)
3. **Lux curve** — exponential saturation maps ambient lux to brightness

All three are multiplicative. PNF and CNF are frozen first, then the lux
curve is calibrated on top.

---

## The model

### Lux curve (implemented, in LightPolicy.cpp)

```
luxBrightness = brMax × (1 - exp(-luxRate × lux))
```

Exponential saturation: rises steeply at low lux, flattens toward brMax.
No normalization to [0,1] needed — the model directly outputs brightness.

### Runtime formula (calcShiftedHi)

```
brightness = luxBrightness
           × calendarShift           (skipped during calibration mode)
           × webMultiplier           (user slider, 0-1)
           × CNF[activeColors]
           × PNF[activePattern]
           → clamped to [brightnessLo, brightnessHi]
           → this clamped value goes to FastLED.setBrightness()
```

During calibration mode:
- calendarShift forced to 1.0 (no calendar interference)
- `lastFastledBrightness` stores the clamped value (= what FastLED receives)
- Sample = lastFastledBrightness / (cnf × pnf)

### Parameters (in globals.csv)

| Parameter | Default | Range | What it does |
|-----------|---------|-------|-------------|
| brMax | 222.0 | [10..500] | Brightness asymptote |
| luxRate | 0.02 | [0.001..0.5] | Saturation rate (higher = faster rise) |
| luxMax | 300.0 | grows | Highest observed lux (high-water mark, defines seed range) |

### Why exponential saturation (not Stevens' power law)

Stevens' power law requires normalization to [0,1] via luxMax, and outputs
a dimensionless fraction that must be scaled. The exponential saturation
model directly maps lux → brightness with only 2 parameters (brMax, luxRate).

- Natural saturation: brightness can't exceed brMax regardless of lux
- No luxMax dependency in the model itself (luxMax is only used for seed spacing)
- Better fit to measured data (R² typically > 0.95 with 7+ real samples)
- Analytical Jacobian enables fast Gauss-Newton fitting on ESP32

---

## PNF — Pattern Normalisation Factor

**Status: implemented**

Compensates for intrinsic brightness differences between patterns.
A "Twinkling Stars" pattern at bri=200 looks dimmer than "Stationary Split"
at bri=200 because fewer LEDs are lit at any given moment.

```
PNF(pattern) = TargetPower / MeanPatternPower
```

Stored per pattern in light_patterns.csv. Calibrated via PNF measurement
at boot (run each pattern, accumulate power, compute ratio).

---

## CNF — Colors Normalisation Factor

**Status: implemented**

Compensates for luminance differences between colors's.
Snow White (#FFFFFF) at bri=200 looks far brighter than Deep Space (#000000)
at bri=200.

```
CNF(colors) = ReferenceColorPower / MeanColorPower
```

Bootstrapped from CIE luminance of the hex values:
`Y = 0.2126×R + 0.7152×G + 0.0722×B` (both colors of the pair, averaged).
Stored per colors in light_colors.csv.

---

## Calibration method

### Web GUI: Cal Lux modal

1. User opens Cal Lux modal → calibration mode activates
2. CalendarShift forced to 1.0 (no calendar interference)
3. User adjusts brightness slider until LEDs look right for current light
4. Presses Sample → firmware captures (lux, FastLED brightness / (cnf×pnf))
5. Repeat at different lux levels (different times of day, curtains open/closed)
6. Press Fit → Gauss-Newton fits brMax and luxRate to collected data
7. Review R² and params → Accept saves to globals.csv
8. Close modal → calibration mode deactivates, brightness restored

### Gauss-Newton fitting (LuxCalibration.cpp)

- 2 parameters: brMax (B), luxRate (r)
- Model: `brightness = B × (1 - exp(-r × lux))`
- Analytical Jacobian: `∂/∂B = 1 - exp(-r×lux)`, `∂/∂r = B × lux × exp(-r×lux)`
- 2×2 normal equation (JᵀWJ)δ = JᵀWe, solved directly
- Weighted: `w = 1/(1+lux)` — low-lux samples count more
- Max 12 iterations, convergence when Δ < 0.001
- Clamps: B ∈ [10, 500], r ∈ [0.001, 0.5]
- Quality metric: R² = 1 - SSres/SStot

### Seeded data (prior from last fit)

At boot and after Accept, seeds are generated from current params:
- 20 synthetic points spread over [0, luxMax] with quadratic spacing
- `nf = 1.0` (seeds represent the pure model, no normalization)
- Seeds provide a prior so the fit stays stable with few real samples
- As real samples accumulate, they outweigh seeds

### luxMax — high-water mark

- `luxMax` is the highest lux ever observed during sampling
- Grows automatically: if a new sample has `lux > luxMax`, luxMax = ceil(lux)
- Saved to globals.csv on Accept
- Used only for seed spacing (the model itself doesn't use luxMax)
- NOT reset by Clear (physical measurement, not a fit parameter)

Calibration runs on **MARMER (188)** only (has lux sensor).
Development on **HOUT (189)** (normalisation code, web GUI).

---

## Ordering rule

1. Freeze PNF and CNF first
2. Then calibrate lux curve
3. If PNF or CNF changes → redo lux calibration

---

## Source documents (historical)

These documents fed into SNB. Refer to plan_snb.md going forward.
Stevens' power law and grid search references in these docs are **obsolete** —
the current model is exponential saturation with Gauss-Newton fitting.

| Document | Role |
|----------|------|
| BRIGHTNESS NORMALISATION.txt | PNF/CNF theory, lux adaptation concept |
| calibration.txt | PNF/CNF measurement procedure, pseudocode |
| plan_b10_lux_calibration.md | Thumbs-up calibration concept (grid search obsolete) |

---

## Invariants

1. **Lux is ambient-only** — measured when all LEDs are OFF (black state)
2. **PNF and CNF are frozen before lux calibration**
3. **No circular recalibration** — if PNF/CNF change, redo lux curve
4. **Sample = what FastLED receives** — clamped brightness / (cnf×pnf), never a model intermediate
5. **CalendarShift disabled during calibration** — user controls brightness via slider only
6. **Channel shifts / white balance are separate** — never merged into global gain
7. **Fades are applied AFTER the combined multiplier**
