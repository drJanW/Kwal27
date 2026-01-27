/**
 * @file SensorManager.h
 * @brief Sensor initialization and reading interface for distance (VL53L1X) and lux (BH1750/VEML7700)
 * @version 251231E
 * @date 2025-12-31
 *
 * Header file defining the SensorManager class and SensorEvent structure.
 * Provides a unified interface for initializing and reading multiple sensors:
 * - Distance sensor (VL53L1X) for proximity detection
 * - Lux sensor (VEML7700) for ambient light measurement
 * Sensor events are queued and can be polled or consumed by other modules.
 */

// lib/SensorManager20251004/SensorManager.h
#pragma once
#include <Arduino.h>
#include "TimerManager.h"

struct SensorEvent {
  uint8_t  type;
  uint8_t  a;
  uint16_t b;
  uint32_t value;
  uint32_t ts_ms;
};

class SensorManager {
public:
  // Sensor init entry points - arm timer for each sensor
  static void beginDistanceSensor();  // VL53L1X distance sensor
  static void beginLuxSensor();       // VEML7700 ambient light sensor (no periodic timer)
  static void beginSensor3();         // Placeholder: board temp/voltage sensor

  // Ready checks
  static bool isDistanceSensorReady();
  static bool isLuxSensorReady();
  static bool isSensor3Ready();
  static bool isDistanceSensorInitFailed();  // True if init failed after max retries
  static bool isLuxSensorInitFailed();       // True if init failed

  static void init(uint32_t ivUpdateMs = 100);

  static void update();                 // door TimerSystem aangeroepen
  static bool readEvent(SensorEvent&);  // ContextManager consumeert

  static void setDistanceMillimeters(float value);
  static float distanceMillimeters();

  static void setAmbientLux(float value);
  static float ambientLux();

  // ConductManager triggers a single lux measurement
  static void performLuxMeasurement();

private:
  static void cb_sensorRead();
  static void addEvent(const SensorEvent& ev);
};
