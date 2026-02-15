/**
 * @file SensorsBoot.cpp
 * @brief Sensor subsystem one-time initialization implementation
 * @version 260215B
 * @date 2026-02-15
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
    // Only init sensors that are declared present in config.txt
    if (Globals::distanceSensorPresent) SensorController::beginDistanceSensor();  // VL53L1X distance
    if (Globals::luxSensorPresent)      SensorController::beginLuxSensor();       // VEML7700 ambient light
    if (Globals::sensor3Present)         SensorController::beginSensor3();         // Board temp/voltage
    
    PL_BOOT("[SensorsBoot] init timers started");
    SensorsPolicy::configure();
}
