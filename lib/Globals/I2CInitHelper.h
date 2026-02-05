/**
 * @file I2CInitHelper.h
 * @brief Generic I2C device initialization with growing retry interval
 * @version 260201A
 * @date 2026-02-01
 */
#pragma once

#include <stdint.h>
#include "Alert/AlertState.h"
#include "Alert/AlertRun.h"

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
    AlertRequest okRequest;      // AlertRequest::RTC_OK
    AlertRequest failRequest;    // AlertRequest::RTC_FAIL
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
