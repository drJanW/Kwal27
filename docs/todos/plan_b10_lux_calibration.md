# Plan B10: Calibrating Lux Sensor

## Status: Planning
## Date: 2026-02-27

---

## Context

The VEML7700 ambient light sensor drives automatic brightness adaptation:
brighter room → LEDs brighter, darker room → LEDs dimmer.

**Current pipeline:**
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
| luxMax | 800.0 | Sensor ceiling — **likely way too high for indoor** |
| luxShiftLo | -10 | Shift at darkest → -10% brightness |
| luxShiftHi | +10 | Shift at brightest → +10% brightness |
| luxGamma | 0.4 | Stevens' exponent (compresses high lux, expands low lux) |
| luxMeasurementDelayMs | 200 | LED blackout before read — **may be too short** |
| luxMeasurementIntervalMs | 2 min | How often to sample ambient |

**Known issues:**
1. VEML7700 uses default gain/integration time from Adafruit library — not tuned for installation
2. `luxMax=800` is a guess; real indoor ambient for the installation is unknown
3. No way to see raw lux values in the web GUI — calibration is blind
4. LED blackout of 200ms may not be enough for VEML7700 integration time
5. Shift range ±10% is conservative — may be too subtle to notice
6. `luxGamma=0.4` is theoretical — needs validation against perceived response
7. HOUT has `lux=0` (sensor not installed), MARMER has `lux=1`

---

## Phase 1: Observability (firmware + web)

**Goal:** See raw lux values so calibration decisions are data-driven.

### 1a. Enrich serial log output
- `cb_luxSensorRead()` already logs `[LuxSensor] %.1f lux` — keep this
- `cb_measureLux()` already logs `[LightRun] Lux=%.1f calShift=%d webMultiplier=%.2f → shiftedHi=%u` — keep this
- **Add:** after `performLuxMeasurement()`, also log VEML7700 gain and integration time settings

### 1b. Add lux to SSE state push
- Add `"lux"` field to `WebGuiStatus::pushState()` JSON
- Source: `SensorController::ambientLux()`
- Also add `"luxShiftPct"` (the computed shift percentage, for tuning visibility)

### 1c. Show lux in web GUI
- Add a small read-only lux display in the status/health area of the web GUI
- Show: raw lux value + computed shift percentage
- No controls needed — just observability

### 1d. Add `/api/sensor/lux` endpoint (optional)
- Returns `{"lux": 42.3, "luxShift": -3, "shiftedHi": 228, "gain": "1x", "integration": "100ms"}`
- Useful for command-line monitoring during calibration: `curl http://189/api/sensor/lux`

**Files touched:**
- `lib/SensorController/SensorController.cpp` — log gain/IT
- `lib/WebInterfaceController/WebGuiStatus.cpp` — add lux to SSE JSON
- `sdroot/webgui-src/js/status.js` — display lux value
- Optionally: `lib/WebInterfaceController/routes/` — new API endpoint

---

## Phase 2: VEML7700 Hardware Configuration

**Goal:** Set optimal gain and integration time for the installation environment.

### 2a. Understand VEML7700 ranges
| Gain | Integration | Resolution | Max lux |
|------|-------------|------------|---------|
| 2x | 800ms | 0.0036 lux/ct | 236 lux |
| 2x | 400ms | 0.0072 lux/ct | 472 lux |
| 1x | 100ms | 0.0576 lux/ct | 3775 lux |
| 1/4 | 100ms | 0.2304 lux/ct | 15099 lux |

Indoor living room: ~50-300 lux. Gallery/museum: ~100-500 lux.
The installation probably sees 10-500 lux range.

### 2b. Configure VEML7700 after `begin()`
In `probeLuxSensor()` or a new `configureLuxSensor()`:
```cpp
veml7700.setGain(VEML7700_GAIN_1);           // 1x gain (good for indoor)
veml7700.setIntegrationTime(VEML7700_IT_100MS); // 100ms integration
```
**Or** make gain/IT configurable via `globals.csv` for field tuning without recompile.

### 2c. Validate blackout timing
- VEML7700 with 100ms integration needs at least ~120ms of stable light to produce an accurate reading
- Current `luxMeasurementDelayMs=200` should be sufficient for 100ms IT
- If IT is increased to 400ms, blackout must be ≥450ms
- **Rule:** `luxMeasurementDelayMs ≥ integration_time + 50ms`

**Files touched:**
- `lib/SensorController/SensorController.cpp` — configure gain/IT in `probeLuxSensor()`
- Optionally: `lib/Globals/Globals.h` + `Globals.cpp` + `globals.csv` — new params

---

## Phase 3: Measure Real Environmental Range

**Goal:** Determine actual luxMin and luxMax for the installation site.

### 3a. Data collection protocol
1. Enable `lux=1` on HOUT (or use MARMER serial log)
2. Set `luxMeasurementIntervalMs` to 10 seconds (fast sampling for calibration)
3. Measure in 4 conditions:
   - **Dark:** lights off, curtains closed → note lux reading
   - **Normal evening:** typical ambient lighting → note lux
   - **Bright day:** curtains open, overhead lights on → note lux
   - **Direct light:** if sunlight can hit the sensor area → note max lux
4. Record readings from serial log or web GUI

### 3b. Set calibrated range
- `luxMin` → floor reading (dark room), probably 0-5 lux
- `luxMax` → ceiling reading (brightest expected), probably 200-500 lux
- Update `globals.csv` with measured values
- **Caution:** don't set luxMax based on one outlier measurement (see github_copilot_refund_request.md — luxMax=52 incident)

### 3c. Consider per-device luxMax
- HOUT and MARMER are in different environments
- If ranges differ significantly, consider per-device `luxMax` in `config.txt`
- **Currently not supported** — would need a `config.txt` parser extension

**Files touched:**
- `sdroot/globals.csv` — update luxMin, luxMax values
- Optionally: `lib/Globals/Globals.cpp` — per-device config.txt parsing for luxMax

---

## Phase 4: Tune Brightness Response Curve

**Goal:** Make brightness changes feel natural and noticeable.

### 4a. Evaluate shift range
- Current ±10% is subtle (brightness 242 × 0.90 = 218, × 1.10 = 242 [capped])
- Consider widening to ±20% or ±30% for more visible impact
- Test: does a noticeable room light change produce a noticeable LED brightness change?

### 4b. Evaluate gamma curve
- `luxGamma=0.4` compresses high lux, expands low lux (matches human perception)
- Lower gamma (0.3) → even more sensitivity at low lux
- Higher gamma (0.5) → more linear response
- Test with Phase 1 observability: check if shift percentage changes appropriately across lux range

### 4c. Consider asymmetric shifts
- Currently luxShiftLo and luxShiftHi are symmetric around 0
- Maybe dark rooms need more brightness reduction than bright rooms need increase
- Example: luxShiftLo=-20, luxShiftHi=+10

### 4d. End-to-end validation
With calibrated values, verify the full chain:
```
Dark room (5 lux)    → luxShift ≈ luxShiftLo → brightness visibly lower
Normal (150 lux)     → luxShift ≈ 0 → brightness at baseline
Bright room (400 lux)→ luxShift ≈ luxShiftHi → brightness visibly higher
```

**Files touched:**
- `sdroot/globals.csv` — tune luxShiftLo, luxShiftHi, luxGamma

---

## Phase 5: Cleanup & Documentation

### 5a. Remove commented-out luxBeta references
- `globals.csv` line 44: `#luxBeta;f;0.005` — dead param, remove
- `Globals.cpp` line 277: comment about luxBeta removal — remove

### 5b. Update globals.csv comments
- Uncomment final calibrated values
- Add comment with measured range: `# Measured: MARMER 3-350 lux, HOUT 5-450 lux` (example)

### 5c. Restore measurement interval
- After calibration, set `luxMeasurementIntervalMs` back to 2 min (or tune if needed)

### 5d. Move b10 off todo sidelined list

---

## Execution Order

| Step | Phase | Effort | Dependency |
|------|-------|--------|------------|
| 1 | 1a-1c: Observability | ~1 hour | None |
| 2 | 2b: Configure VEML7700 | ~15 min | None |
| 3 | 3a: Data collection | ~30 min hands-on | Phase 1 + 2 |
| 4 | 3b: Set calibrated range | ~10 min | Phase 3 data |
| 5 | 4a-4c: Tune response | ~30 min iterative | Phase 3 |
| 6 | 4d: Validate | ~15 min | Phase 4 |
| 7 | 5a-5d: Cleanup | ~15 min | Phase 4 validated |

**Total: ~2.5 hours** (mostly hands-on measurement time)

---

## Risks

| Risk | Mitigation |
|------|------------|
| Sensor placement inside sculpture blocks ambient light | Consider light pipe or repositioning sensor |
| LED bleed during blackout corrupts reading | Increase `luxMeasurementDelayMs`; verify LED current actually reaches 0 during blackout |
| VEML7700 saturates in direct sunlight | Use lower gain setting; clamp at luxMax |
| Setting luxMax too low from one measurement | Collect data over multiple conditions, use conservative (high) ceiling |
| Calibration differs between HOUT and MARMER | Per-device luxMax if needed; or accept one config that works "well enough" for both |

---

## Open Questions

1. **Physical placement**: Where exactly is the VEML7700 mounted in each sculpture? Does the jellyfish body block ambient light?
2. **HOUT sensor**: HOUT has `lux=0` — is there a VEML7700 physically installed on HOUT, or only on MARMER?
3. **Desired effect magnitude**: Should brightness adaptation be subtle (±10%) or dramatic (±30%+)?
4. **Gain/IT preference**: Configurable via globals.csv (flexible, more code) or hardcoded (simple, one fewer CSV param)?
