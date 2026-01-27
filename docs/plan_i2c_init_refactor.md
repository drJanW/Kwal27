# I2C Init Refactor Plan

## Probleem

Elke I2C device (RTC, VL53L1X, VEML7700, Sensor3, toekomstige Sensor4-6) heeft identieke init logica:
- Timer met growing interval
- Retry count naar NotifyState
- Try device.begin()
- Succes/fail reporting

Code is 3x gekopieerd met minimale variatie. Elke nieuwe sensor = 30 regels copy-paste.

## Oplossing

Generieke `I2CInitHelper` in lib/ContextManager of lib/Globals.

## Interface

```cpp
// Callback type: returns true if device.begin() succeeded
using I2CProbeFunc = bool(*)();

struct I2CInitConfig {
    const char* name;           // "RTC", "Distance", "Lux", "Sensor3"
    StatusComponent comp;       // SC_RTC, SC_DIST, SC_LUX, SC_SENSOR3
    I2CProbeFunc probe;         // []() { return rtc.begin(); }
    int maxRetries;             // 15
    uint32_t startDelayMs;      // 1000
    NotifyIntent okIntent;      // NotifyIntent::RTC_OK
    NotifyIntent failIntent;    // NotifyIntent::RTC_FAIL
};

namespace I2CInitHelper {
    void start(const I2CInitConfig& cfg);
    bool isReady(StatusComponent comp);
    bool isFailed(StatusComponent comp);
}
```

## Gebruik

```cpp
// ClockPolicy.cpp
void begin() {
    I2CInitHelper::start({
        "RTC", SC_RTC,
        []() { return rtc.begin(); },
        10, 1000, 1.5f,
        NotifyIntent::RTC_OK, NotifyIntent::RTC_FAIL
    });
}

// SensorManager.cpp  
void beginDistanceSensor() {
    I2CInitHelper::start({
        "Distance", SC_DIST,
        []() { return VL53L1X_begin(); },
        15, 1000, 1.5f,
        NotifyIntent::DISTANCE_SENSOR_OK, NotifyIntent::DISTANCE_SENSOR_FAIL
    });
}

// Sensor3 (toekomstig)
void beginSensor3() {
    I2CInitHelper::start({
        "Sensor3", SC_SENSOR3,
        []() { return sensor3.begin(); },
        10, 1000, 1.75f,
        NotifyIntent::SENSOR3_OK, NotifyIntent::SENSOR3_FAIL
    });
}
```

## Uitbreiding Sensor4-6

Voor toekomstige sensors moeten toegevoegd worden:

1. **NotifyState.h** - enum StatusComponent uitbreiden:
   ```cpp
   SC_SENSOR4,
   SC_SENSOR5,
   SC_SENSOR6,
   SC_COUNT  // blijft laatste
   ```

2. **NotifyIntent** - OK/FAIL intents toevoegen

3. **health.js** - FLAGS array synchroon houden

4. **ContextStatus.h** - STATUS_SENSOR4_OK etc. toevoegen (indien RGB flash gewenst)

Met I2CInitHelper: nieuwe sensor = 6 regels config ipv 30 regels copy-paste.

## Intern

- Array van actieve configs (max 8)
- Eén generieke callback die config opzoekt via TimerManager callback pointer
- Of: per-device static callback die generieke helper aanroept

## Voordelen

1. Één plek voor retry logica
2. Nieuwe I2C devices: 6 regels ipv 30
3. Consistente logging
4. Makkelijk aanpasbare retry parameters
5. Sensor3-6 uitbreiding triviaal

## Status

**TODO** - niet urgent, huidige code werkt
