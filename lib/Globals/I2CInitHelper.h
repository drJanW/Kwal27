/**
 * @file I2CInitHelper.h
 * @brief Generic I2C device initialization with growing retry interval
 * @version 0101F5A
 * @date 2026-01-01
 *
 * Provides unified initialization logic for I2C devices (RTC, sensors).
 * Each device gets retries with growing interval and NotifyState updates.
 * Replaces duplicate init code in ClockPolicy and SensorManager.
 */

#pragma once

#include <stdint.h>
#include "Notify/NotifyState.h"
#include "Notify/NotifyRun.h"

// Callback type: returns true if device.begin() succeeded
using I2CProbeFunc = bool(*)();

// Timer callback type
using TimerCallback = void(*)();

struct I2CInitConfig {
    const char* name;           // "RTC", "Distance", "Lux", "Sensor3"
    StatusComponent comp;       // SC_RTC, SC_DIST, SC_LUX, SC_SENSOR3
    I2CProbeFunc probe;         // []() { return rtc.begin(); }
    uint8_t maxRetries;         // 10, 14, 13 (positive count)
    uint32_t startDelayMs;      // 1000
    float growth;               // 1.5f (interval multiplier per retry)
    NotifyIntent okIntent;      // NotifyIntent::RTC_OK
    NotifyIntent failIntent;    // NotifyIntent::RTC_FAIL
};

namespace I2CInitHelper {
    // Register device and start timer with per-device callback
    void start(const I2CInitConfig& cfg, TimerCallback cb);
    
    // Called by per-device callback - does probe + reporting
    void tryInit(StatusComponent comp);
    
    // Query device state
    bool isReady(StatusComponent comp);
    bool isFailed(StatusComponent comp);
}
