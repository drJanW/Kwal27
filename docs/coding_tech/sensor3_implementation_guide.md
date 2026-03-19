# Sensor3 Implementation Guide

> Version: 260319A | Updated: 2026-03-19

## What is Sensor3?

Sensor3 is a placeholder for a future I2C sensor, likely:
- Board temperature sensor (ESP32 internal temp or external I2C)
- Voltage monitor
- Other environmental sensor

## Current State (v260319A)

Sensor3 is **disabled by design** via `Globals::sensor3Present = false`.

### Runtime Presence Architecture

Since v260207, the system uses runtime `inline static bool` flags in Globals.h:

```cpp
// Globals.h
inline static bool luxSensorPresent      = true;
inline static bool distanceSensorPresent = false;
inline static bool sensor3Present        = false;  // placeholder
inline static bool audioPresent          = true;
inline static bool nasPresent            = true;
inline static bool rtcPresent            = true;
```

These are CSV-overridable via globals.csv. When `sensor3Present = false`:
- No init attempt
- No failure flash
- No TTS announcement
- AlertState marks SC_SENSOR3 as ABSENT
- WebGUI health shows dash

### Existing stubs/placeholders

| File | What | Status |
|------|------|--------|
| Globals.h | `sensor3Present = false` | Runtime presence flag |
| HWconfig.h | `SENSOR3_DUMMY_TEMP 25.0f` | Fallback value |
| Globals.h/cpp | `sensor3DummyTemp` | Configurable via CSV |
| globals.csv | `#sensor3DummyTemp;f;25.0` | Commented out |
| SensorsBoot.cpp | Guard: skip init if `!sensor3Present` | No init attempt |
| AlertRGB.cpp | Guard: skip flash if `!sensor3Present` | No flash |
| AlertState.h | `isPresent(SC_SENSOR3)` | Returns false (ABSENT) |
| health.js | WebGUI shows dash for absent | Active |
| SpeakRun.cpp | TTS "Sensor drie ontbreekt" | Not triggered when absent |

## Implementation Steps (when hardware becomes available)

### 1. Choose I2C sensor

Examples:
- **TMP117** — precision temperature
- **INA219** — current/voltage monitor
- **BMP280** — pressure/temperature

### 2. Enable in globals.csv

```csv
sensor3Present;b;true;activate sensor3 hardware
```

Or set `Globals::sensor3Present = true` in Globals.h as new default.

### 3. SensorController.cpp — add init callback

Follow the same pattern as VL53L1X and VEML7700:

```cpp
#include <TMP117.h>

namespace {
    TMP117 sensor3;
    
    void cb_sensor3Init() {
        if (sensor3Ready || sensor3InitFailed) return;
        
        uint8_t remaining = timers.remaining();
        AlertState::set(COMP_SENSOR3, remaining);
        
        if (sensor3.begin()) {
            sensor3Ready = true;
            TimerManager::instance().cancel(cb_sensor3Init);
            PL("[SensorController] Sensor3 (TMP117) ready");
            AlertState::setOk(COMP_SENSOR3, true);
            AlertRun::report(AlertRequest::SENSOR3_OK);
            return;
        }
        
        if (abs(remaining) == 1) {
            sensor3InitFailed = true;
            AlertRun::report(AlertRequest::SENSOR3_FAIL);
            PL("[SensorController] Sensor3 gave up after retries");
        }
    }
}

void SensorController::beginSensor3() {
    TimerManager::instance().create(1000, 10, cb_sensor3Init, 1.5f);
}
```

### 4. Add read function

```cpp
// SensorController.h
static float getSensor3Value();

// SensorController.cpp
float SensorController::getSensor3Value() {
    if (!sensor3Ready) return Globals::sensor3DummyTemp;
    return sensor3.readTemperature();
}
```

### 5. Activate globals.csv entry

```csv
sensor3DummyTemp;f;25.0;fallback temp when sensor3 absent
```

## Reminders

1. **I2C address conflict check** — scan bus first
2. **Retry pattern** — use `create(1000, 10, cb, 1.5f)` for exponential retries
3. **TTS text** — "Sensor drie ontbreekt" already exists in SpeakRun.cpp
4. **Flash color** — `AlertPolicy::COLOR_SENSOR3` already defined

## Architecture Pattern

```
SensorsBoot::configure()
    +-- SensorController::beginSensor3()
        +-- TimerManager::create(..., cb_sensor3Init)
              +-- cb_sensor3Init() [growing interval retries]
                    +-- Success: AlertRun::report(SENSOR3_OK)
                    +-- Fail:    AlertRun::report(SENSOR3_FAIL)
                                   +-- AlertState marks FAILED
                                   +-- speakFailure() + RGB flash
```
