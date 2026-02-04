// RTCController.h - Hardware RTC (DS3231) control
// Only this module talks to I2C/RTC hardware
// @version 251231E
// @date 2025-12-31
#pragma once

#include <Arduino.h>
#include <RTClib.h>

class PRTClock;

namespace RTCController {

inline RTC_DS3231 rtc;

// Hardware initialization
void begin();

// Check if RTC hardware is available
bool isAvailable();

// Read time from RTC hardware into PRTClock
bool readInto(PRTClock& clock);

// Write time from PRTClock to RTC hardware
void writeFrom(const PRTClock& clock);

// Get temperature from RTC (NAN if unavailable)
float getTemperature();

// Check if RTC lost power (battery dead/removed)
bool wasPowerLost();

} // namespace RTCController
