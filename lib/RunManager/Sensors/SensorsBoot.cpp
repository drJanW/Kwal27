/**
 * @file SensorsBoot.cpp
 * @brief Sensor subsystem one-time initialization implementation
 * @version 251231E
 * @date 2025-12-31
 *
 * Arms sensor init timers via SensorManager. Each sensor's init callback
 * reports OK/FAIL directly via NotifyRun - no polling required.
 */

#include "SensorsBoot.h"
#include "Globals.h"
#include "SensorManager.h"
#include "SensorsPolicy.h"

// Sensor status reporting is handled by SensorManager init callbacks directly
// No polling needed - callbacks report OK/FAIL via NotifyRun::report()

namespace {
} // namespace

void SensorsBoot::plan() {
    // I2C already initialized in RunManager::begin()
    // Each begin* arms its own retry timer that reports OK/FAIL via NotifyRun
    // Only init sensors that are declared PRESENT in HWconfig.h
    if (DISTANCE_SENSOR_PRESENT) SensorManager::beginDistanceSensor();  // VL53L1X distance
    if (LUX_SENSOR_PRESENT)      SensorManager::beginLuxSensor();       // VEML7700 ambient light
    if (SENSOR3_PRESENT)         SensorManager::beginSensor3();         // Board temp/voltage
    
    PL("[SensorsBoot] Sensor init timers started");
    SensorsPolicy::configure();
}
