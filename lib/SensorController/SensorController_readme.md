# SensorController — Sensor Hardware Abstraction Layer

**Role in Kwal27:** Reads physical sensors (ambient light via BH1750/VEML7700, distance via VL53L1X ToF, motion via MPU9250 IMU) and publishes normalized readings as events. The lux sensor directly drives automatic brightness calibration; the distance sensor triggers proximity-based interactions.

## Files

### SensorController.h / SensorController.cpp
Central sensor coordinator. Owns the sensor reading timer and event queue. API:
- `beginDistanceSensor()` / `beginLuxSensor()` / `beginSensor3()` — arm initialization timers for each sensor
- `init(ivUpdateMs)` — start periodic sensor polling at given interval (default 100 ms)
- `update()` — called by TimerManager each tick; polls sensors, emits events
- `readEvent(SensorEvent&)` — consume next queued event (ContextController reads these)
- `setDistanceMillimeters()/distanceMillimeters()` — current distance value
- `setAmbientLux()/ambientLux()` — current lux value
- `performLuxMeasurement()` — trigger a single lux reading on demand (used by calibration)

`SensorEvent` struct carries: type, auxiliary bytes, value, and timestamp.

### BH1750.h
Minimal BH1750 ambient light sensor driver (I2C address 0x23 or 0x5C). Header-only class providing `begin()`, `readLux()`, and power management. Supports Continuous High-Res mode (1 lx resolution, ~120 ms conversion). Alternative to VEML7700 — `beginLuxSensor()` selects the detected sensor.

### VL53L1X.h / VL53L1X.cpp
Thin-wrapper around SparkFun VL53L1X time-of-flight distance sensor library (I2C address 0x29). API:
- `VL53L1X_begin(address, bus, timingBudgetMs, longRange)` — initialize with timing budget (20–100+ ms) and range mode
- `readVL53L1X()` — returns distance in mm, or NAN if no new sample ready

### MPU9250.h / MPU9250.cpp
9-axis motion sensor driver (accelerometer + gyroscope via MPU6050 I2Cdevlib). Configurable accelerometer range (2/4/8/16G) and gyroscope range (250/500/1000/2000 DPS). Currently unused in normal operation (`hwStatus` bit may not be set, or sensor is `.off`); retained for potential future use.