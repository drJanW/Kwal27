/**
 * @file ClockPolicy.h
 * @brief RTC/NTP clock business logic
 * @version 251231E
 * @date 2025-12-31
 *
 * Contains business logic for clock operations. Handles DS3231 RTC detection,
 * time seeding from RTC, and synchronizing RTC from NTP.
 */

#pragma once

#include <Arduino.h>

class PRTClock;

namespace ClockPolicy {

void begin();
bool isRtcAvailable();
bool seedClockFromRTC(PRTClock &clock);
void syncRTCFromClock(const PRTClock &clock);

}
