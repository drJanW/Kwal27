# SensorController

> Version: 260319A | Updated: 2026-03-19

Thin coordinator for sensor drivers. Timer-driven polling, no ISRs.

## Files

| File | Version | Purpose |
|------|---------|---------|
| `SensorController.h/.cpp` | 260215B | Sensor init and reading interface (distance + lux) |
| `SensorManager.cpp` | — | Sensor lifecycle management (implementation-only, no .h) |
| `BH1750.h/.cpp` | 260205A | Legacy ambient light sensor driver (replaced by VEML7700) |
| `VL53L1X.h/.cpp` | 260215B | Time-of-flight distance sensor driver |
| `MPU9250.h` / `.cpp.off` | 260202A | Accelerometer/gyro driver (disabled) |

## Active Sensors

| Sensor | Driver | I2C Addr | Purpose |
|--------|--------|----------|---------|
| **VEML7700** | `Adafruit_VEML7700` (external lib) | 0x10 | Ambient lux |
| **VL53L1X** | custom (`VL53L1X.h`) | 0x29 | Time-of-flight distance (mm) |
| ~~BH1750~~ | `BH1750.h` | 0x23 | Legacy lux sensor, files still present but unused |
| ~~MPU9250~~ | `MPU9250.h` | — | Disabled (`.cpp.off`) |

Sensor presence is controlled by `*_PRESENT` flags in `HWconfig.h`:
- `LUX_SENSOR_PRESENT`, `DISTANCE_SENSOR_PRESENT`, `SENSOR3_PRESENT`
- Absent sensors: no init, no error flash, status = `--`

## API

```cpp
// SensorController.h (all static)
void beginDistanceSensor();
void beginLuxSensor();
void beginSensor3();
void init(uint32_t ivUpdateMs = 100);     // Start polling timer
void update();                             // Called by timer

// Readings
void setDistanceMillimeters(float value);
float distanceMillimeters();
void setAmbientLux(float value);
float ambientLux();
void performLuxMeasurement();              // On-demand lux read

// Events
bool readEvent(SensorEvent& out);

// BH1750.h (legacy, unused)
bool begin(TwoWire& w, uint8_t addr, uint8_t preset);
bool isReady() const;

// VL53L1X.h
bool VL53L1X_begin(uint8_t address, TwoWire& bus, uint16_t timingBudgetMs, bool longRange);
float readVL53L1X();
```

## Architecture

```
SensorController (coordinator)
    |
    +-- VEML7700 (external Adafruit lib)  --> ambientLux()
    +-- VL53L1X (custom driver)           --> distanceMillimeters()
    |
    v
RunManager/Sensors/ (SensorsBoot, SensorsRun, SensorsPolicy, SensorDirector)
    |
    v
ContextController (consumes sensor data)
```

## Timing

- Poll period set via `init(period_ms)`, typical 100ms
- No `delay()`; all timing via TimerManager
- Lux measurements can be triggered on-demand via `performLuxMeasurement()`