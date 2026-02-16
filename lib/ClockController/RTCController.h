/**
 * @file RTCController.h
 * @brief RTCController implementation
 * @version 260216A
 * @date 2026-02-16
 */
// RTCController.h - Hardware RTC (DS3231) control
// Only this module talks to I2C/RTC hardware
#pragma once

#include <Arduino.h>
#include <RTClib.h>

class PRTClock;

namespace RTCController {

// Hardware initialization
void begin();

// Check if RTC hardware is available
bool isAvailable();

// Read time from RTC hardware into PRTClock
bool readInto(PRTClock& clock);

// Lightweight tick read: H:M:S + date only (no DoW/DoY/moon/settimeofday)
bool readTime(PRTClock& clock);

// Write time from PRTClock to RTC hardware
void writeFrom(const PRTClock& clock);

// Get temperature from RTC (NAN if unavailable)
float getTemperature();

// Check if RTC lost power (battery dead/removed)
bool wasPowerLost();

} // namespace RTCController
