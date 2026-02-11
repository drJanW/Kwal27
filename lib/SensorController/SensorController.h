/**
 * @file SensorController.h
 * @brief Sensor initialization and reading interface for distance (VL53L1X) and lux (BH1750/VEML7700)
 * @version 260202A
 $12026-02-05
 */
// lib/SensorController20251004/SensorController.h
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

class SensorController {
public:
  // Sensor init entry points - arm timer for each sensor
  static void beginDistanceSensor();  // VL53L1X distance sensor
  static void beginLuxSensor();       // VEML7700 ambient light sensor (no periodic timer)
  static void beginSensor3();         // Placeholder: board temp/voltage sensor

  static void init(uint32_t ivUpdateMs = 100);

  static void update();                 // door TimerSystem aangeroepen
  static bool readEvent(SensorEvent&);  // ContextController consumeert

  static void setDistanceMillimeters(float value);
  static float distanceMillimeters();

  static void setAmbientLux(float value);
  static float ambientLux();

  // RunManager triggers a single lux measurement
  static void performLuxMeasurement();

private:
  static void cb_sensorRead();
  static void addEvent(const SensorEvent& ev);
};
