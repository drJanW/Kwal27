# Sensor3 Implementation Guide

## Wat is Sensor3?

Sensor3 is een placeholder voor een toekomstige I2C sensor, waarschijnlijk:
- Board temperatuur sensor (ESP32 interne temp of externe I2C)
- Voltage monitor
- Andere omgevingssensor

## Huidige staat (v260104+)

Sensor3 is **disabled by design** via `SENSOR3_PRESENT false` in `HWconfig.h`.

### Hardware Presence Architecture

Sinds v260104 gebruikt het systeem `*_PRESENT` defines in `HWconfig.h`:
- **Absent hardware** (`*_PRESENT false`): geen init, geen flash, geen reminders, status = `—`
- **Present hardware** (`*_PRESENT true`): normale init met retries, flash bij fail

```cpp
// HWconfig.h
#define RTC_PRESENT false              // DS3231 RTC module
#define DISTANCE_SENSOR_PRESENT true   // VL53L1X ToF sensor
#define LUX_SENSOR_PRESENT false       // VEML7700 ambient light sensor
#define SENSOR3_PRESENT false          // Board temp sensor (placeholder)
```

### Bestaande stubs/placeholders:

| File | Wat | Status |
|------|-----|--------|
| `HWconfig.h` | `SENSOR3_PRESENT false` | **NEW**: Hardware presence flag |
| `HWconfig.h` | `SENSOR3_DUMMY_TEMP 25.0f` | Fallback waarde |
| `Globals.h/cpp` | `sensor3DummyTemp` | Configureerbaar via CSV |
| `globals.csv` | `#sensor3DummyTemp;f;25.0` | Uitgecommentarieerd |
| `SensorsBoot.cpp` | Guard: skip init if `!SENSOR3_PRESENT` | **NEW**: Geen init poging |
| `AlertRGB.cpp` | Guard: skip flash if `!SENSOR3_PRESENT` | **NEW**: Geen flash |
| `AlertState.cpp` | `isPresent(SC_SENSOR3)` | **NEW**: Returns false |
| `health.js` | WebGUI shows `—` for absent | **NEW**: v0104A |
| `SpeakRun.cpp` | TTS "Sensor drie ontbreekt" | Klaar (niet getriggerd als absent) |

## Implementatie stappenplan (wanneer hardware beschikbaar)

### 1. Hardware keuze

Kies I2C sensor, bijv:
- **TMP117** - precisie temperatuur
- **INA219** - stroom/voltage monitor
- **BMP280** - druk/temp

### 2. HWconfig.h aanpassen

```cpp
#define SENSOR3_PRESENT true   // Activeer hardware (was false)
```

### 3. SensorManager.cpp aanpassen

Vervang placeholder door echte init:

```cpp
// Include sensor library
#include <TMP117.h>  // of andere

namespace {
    TMP117 sensor3;  // Instantie
    
    void cb_sensor3Init() {
        if (sensor3Ready || sensor3InitFailed) return;
        
        int remaining = TimerManager::instance().getRepeatCount(cb_sensor3Init);
        if (remaining != -1)
            AlertState::set(COMP_SENSOR3, abs(remaining));
        
        if (sensor3.begin()) {
            sensor3Ready = true;
            TimerManager::instance().cancel(cb_sensor3Init);
            PL("[SensorManager] Sensor3 (TMP117) ready");
            AlertState::setOk(COMP_SENSOR3, true);
            AlertRun::report(AlertRequest::SENSOR3_OK);
            return;
        }
        
        if (abs(remaining) == 1) {
            sensor3InitFailed = true;
            AlertRun::report(AlertRequest::SENSOR3_FAIL);
            PL("[SensorManager] Sensor3 gave up after retries");
        }
    }
}

void SensorManager::beginSensor3() {
    TimerManager::instance().create(1000, 10, cb_sensor3Init, 1.5f);  // 10 exp retries
    PL("[SensorManager] Sensor3 init, max 10 retries with growing interval");
}
```

### 4. Read functie toevoegen

```cpp
// SensorManager.h
static float getSensor3Value();

// SensorManager.cpp
float SensorManager::getSensor3Value() {
    if (!sensor3Ready) return Globals::sensor3DummyTemp;
    return sensor3.readTemperature();
}
```

### 5. AlertState.cpp reset() aanpassen

Verwijder deze regel (sensor3 start nu als "nog niet geïnitialiseerd"):
```cpp
setOk(COMP_SENSOR3, false);  // DELETE: was placeholder
```

### 6. speakFailures() check

`SpeakRun::speakFailures()` bevat al:
```cpp
if (!AlertState::isOk(COMP_SENSOR3)) speak(SpeakRequest::SENSOR3_FAIL);
```

Dit werkt automatisch zodra sensor3 faalt.

### 7. globals.csv activeren

Uncomment en pas aan:
```csv
sensor3DummyTemp;f;25.0;fallback temp when sensor3 absent
```

## Niet vergeten

1. **I2C adres conflict check** - scan bus eerst
2. **Retry count** - volg patroon: `-10` voor 10 retries met growing interval
3. **TTS tekst** - "Sensor drie ontbreekt" al klaar, of wijzig in SpeakRun.cpp
4. **Flash kleur** - `AlertPolicy::COLOR_SENSOR3` al gedefinieerd

## Architectuur patroon

Volg exact hetzelfde patroon als VL53L1X en VEML7700:

```
SensorsBoot::configure()
  └── SensorManager::beginSensor3()
        └── TimerManager::create(..., cb_sensor3Init)
              └── cb_sensor3Init() [growing interval retries]
                    ├── Success: AlertRun::report(SENSOR3_OK)
                    └── Fail: AlertRun::report(SENSOR3_FAIL)
                                └── AlertState::setSensor3Status(false)
                                      └── speakFailure() + RGB flash
```

## Toekomstig: F2 I2C Init refactor

Zie `docs/plan_i2c_init_refactor.md` - als die geïmplementeerd is, wordt sensor3 init nog simpeler:

```cpp
void SensorManager::beginSensor3() {
    I2CInitHelper::start({
        "Sensor3", COMP_SENSOR3,
        []() { return sensor3.begin(); },
        -10, 1000,
        AlertRequest::SENSOR3_OK, AlertRequest::SENSOR3_FAIL
    });
}
```
