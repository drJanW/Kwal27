/**
 * @file SensorsBoot.cpp
 * @brief Sensor subsystem one-time initialization implementation
 * @version 260201A
 * @date 2026-02-01
 */
#include "SensorsBoot.h"
#include "Globals.h"
#include "SensorController.h"
#include "SensorsPolicy.h"

// Sensor status reporting is handled by SensorController init callbacks directly
// No polling needed - callbacks report OK/FAIL via AlertRun::report()

namespace {
} // namespace

void SensorsBoot::plan() {
    // I2C already initialized in RunManager::begin()
    // Each begin* arms its own retry timer that reports OK/FAIL via AlertRun
    // Only init sensors that are declared PRESENT in HWconfig.h
    if (DISTANCE_SENSOR_PRESENT) SensorController::beginDistanceSensor();  // VL53L1X distance
    if (LUX_SENSOR_PRESENT)      SensorController::beginLuxSensor();       // VEML7700 ambient light
    if (SENSOR3_PRESENT)         SensorController::beginSensor3();         // Board temp/voltage
    
    PL_BOOT("[SensorsBoot] init timers started");
    SensorsPolicy::configure();
}
