/**
 * @file SensorManager.cpp
 * @brief Sensor initialization and reading for distance (VL53L1X) and lux (BH1750/VEML7700)
 * @version 251231E
 * @date 2025-12-31
 *
 * This file implements the SensorManager class which coordinates multiple sensors
 * on the ESP32. It handles initialization, periodic reading, and event queuing for:
 * - VL53L1X time-of-flight distance sensor (detects presence/proximity)
 * - VEML7700 ambient light sensor (measures illuminance in lux)
 * The manager uses a timer-based update mechanism and provides a unified event
 * interface for sensor data to the rest of the system.
 */

// lib/SensorManager20251004/SensorManager.cpp
#define LOCAL_LOG_LEVEL LOG_LEVEL_INFO
#include "SensorManager.h"
#include "Globals.h"
#include "I2CInitHelper.h"
#include <atomic>
#include <math.h>
#if DISTANCE_SENSOR_PRESENT
#include "VL53L1X.h"
#ifndef VL53L1X_DEBUG
#define VL53L1X_DEBUG 0
#endif

#if VL53L1X_DEBUG
#define SENSOR_VL53_LOG(...) PF(__VA_ARGS__)
#else
#define SENSOR_VL53_LOG(...) do {} while (0)
#endif
#endif

#include <Adafruit_VEML7700.h>
#include "Notify/NotifyConduct.h"
#include "Notify/NotifyState.h"

namespace {
  constexpr uint8_t Q_MASK = 0x0F;          // queue size 16
  SensorEvent q[Q_MASK + 1];
  uint8_t qHead = 0, qTail = 0;

  uint32_t ivUpdateMs = 0;
  constexpr uint32_t SENSOR_RETRY_INTERVAL_MS = 500;

  uint32_t baseIntervalMs = 100;  // Updated from Globals at init
  bool hasDistance = false;
  float lastDistanceMm = 0.0f;

  std::atomic<bool> distanceSampleValid{false};
  std::atomic<float> distanceSampleMm{0.0f};
  std::atomic<float> ambientLuxVal{0.0f};
  std::atomic<float> boardTempVal{0.0f};
  std::atomic<float> boardVoltVal{0.0f};

  Adafruit_VEML7700 veml7700;  // Ambient light sensor instance

#if DISTANCE_SENSOR_PRESENT
  uint32_t vlLastMs   = 0;
#endif

  inline uint8_t inc(uint8_t i){ return uint8_t((i+1)&Q_MASK); }
  inline uint32_t now_ms(){ return (uint32_t)millis(); }

  // Probe functions for I2CInitHelper
  bool probeDistanceSensor() {
#if DISTANCE_SENSOR_PRESENT
    return VL53L1X_begin();
#else
    return false;
#endif
  }

  bool probeLuxSensor() {
    return veml7700.begin();
  }

  // Per-device init callbacks
  void cb_distanceInit() {
    I2CInitHelper::tryInit(SC_DIST);
  }

  void cb_luxInit() {
    I2CInitHelper::tryInit(SC_LUX);
  }

  // Lux sensor read callback
  void cb_luxSensorRead() {
    if (!I2CInitHelper::isReady(SC_LUX)) return;
    float lux = veml7700.readLux();
    SensorManager::setAmbientLux(lux);
#if LOCAL_LOG_LEVEL >= LOG_LEVEL_INFO
    PF("[LuxSensor] %.1f lux\n", lux);
#endif
  }

}

void SensorManager::addEvent(const SensorEvent& ev){
  uint8_t n = inc(qHead);
  if (n == qTail) qTail = inc(qTail);
  q[qHead] = ev;
  qHead = n;
}

void SensorManager::cb_sensorRead(){
  SensorManager::update();
}

void SensorManager::setDistanceMillimeters(float value) {
  distanceSampleMm.store(value, std::memory_order_relaxed);
  distanceSampleValid.store(true, std::memory_order_relaxed);
}

float SensorManager::distanceMillimeters() {
  if (!I2CInitHelper::isReady(SC_DIST)) return static_cast<float>(Globals::distanceSensorDummyMm);  // Fallback: "far away"
  return distanceSampleMm.load(std::memory_order_relaxed);
}

void SensorManager::setAmbientLux(float value) {
  ambientLuxVal.store(value, std::memory_order_relaxed);
}

float SensorManager::ambientLux() {
  return ambientLuxVal.load(std::memory_order_relaxed);
}

// Sensor init entry points
void SensorManager::beginDistanceSensor() {
  // Start the polling timer (once, for all sensors)
  static bool pollingStarted = false;
  if (!pollingStarted) {
    SensorManager::init(Globals::sensorBaseDefaultMs);
    pollingStarted = true;
  }
#if DISTANCE_SENSOR_PRESENT
  // Init delay and growth configurable via globals.csv
  // Timer fires with growing interval: delay -> delay*growth -> delay*growth^2 -> ...
  I2CInitHelper::start({
      "Distance", SC_DIST,
      probeDistanceSensor,
      14, Globals::distanceSensorInitDelayMs, Globals::distanceSensorInitGrowth,
      NotifyIntent::DISTANCE_SENSOR_OK, NotifyIntent::DISTANCE_SENSOR_FAIL
  }, cb_distanceInit);
#else
  NotifyState::setStatusOK(SC_DIST, false);
  NotifyConduct::report(NotifyIntent::DISTANCE_SENSOR_FAIL);
  PL("[SensorManager] DistanceSensor (VL53L1X) disabled");
#endif
}

void SensorManager::beginLuxSensor() {
  // Init delay and growth configurable via globals.csv
  // Timer fires with growing interval: delay -> delay*growth -> delay*growth^2 -> ...
  I2CInitHelper::start({
      "Lux", SC_LUX,
      probeLuxSensor,
      13, Globals::luxSensorInitDelayMs, Globals::luxSensorInitGrowth,
      NotifyIntent::LUX_SENSOR_OK, NotifyIntent::LUX_SENSOR_FAIL
  }, cb_luxInit);
}

void SensorManager::performLuxMeasurement() {
  // Called by ConductManager after LEDs are off and delay elapsed
  cb_luxSensorRead();
}

void SensorManager::beginSensor3() {
  // Placeholder: no hardware yet
  PL("[SensorManager] Sensor3 (board) placeholder - no hardware");
}

// Sensor ready checks
bool SensorManager::isDistanceSensorReady() {
  return I2CInitHelper::isReady(SC_DIST);
}

bool SensorManager::isLuxSensorReady() {
  return I2CInitHelper::isReady(SC_LUX);
}

bool SensorManager::isSensor3Ready() {
  return false;  // Placeholder
}

bool SensorManager::isDistanceSensorInitFailed() {
  return I2CInitHelper::isFailed(SC_DIST);
}

bool SensorManager::isLuxSensorInitFailed() {
  return I2CInitHelper::isFailed(SC_LUX);
}

void SensorManager::init(uint32_t ivMs)
{
  baseIntervalMs = ivMs ? ivMs : Globals::sensorBaseDefaultMs;
  hasDistance = false;
  lastDistanceMm = 0.0f;

  auto &tm = timers;
  if (!tm.create(baseIntervalMs, 0, SensorManager::cb_sensorRead)) {
    PL("[SensorManager] Failed to create sensor polling timer");
    return;
  }
  ivUpdateMs = baseIntervalMs;

  if (ivUpdateMs == 0) {
    PF("[SensorManager] Polling interval not scheduled\n");
  }
}

void SensorManager::update()
{
  uint32_t now = now_ms();
  (void)now;

#if DISTANCE_SENSOR_PRESENT
  if (!I2CInitHelper::isReady(SC_DIST)) {
    return;  // Wait for init to succeed
  }

  float distanceMm = readVL53L1X();
  if (!isnan(distanceMm)) {
    float deltaMm = 0.0f;
    if (hasDistance) {
      deltaMm = fabsf(distanceMm - lastDistanceMm);
    } else {
      hasDistance = true;
    }
    (void)deltaMm;

    lastDistanceMm = distanceMm;

    // Policy will handle fast polling decisions based on distance changes
    SensorManager::setDistanceMillimeters(distanceMm);
    vlLastMs = now_ms();

    SensorEvent dist{};
    dist.type  = 0x30; // TYPE_DISTANCE_MM
    dist.value = static_cast<uint32_t>(distanceMm);
    dist.ts_ms = vlLastMs;
    addEvent(dist);
    SENSOR_VL53_LOG("[SensorManager] VL53L1X distance=%.0fmm\n", distanceMm);
  }
#endif
}

bool SensorManager::readEvent(SensorEvent& out)
{
  if (qHead == qTail) return false;
  out = q[qTail];
  qTail = inc(qTail);
  return true;
}
