/**
 * @file ClockPolicy.h
 * @brief RTC/NTP clock business logic
 * @version 260204A
 * @date 2026-02-04
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
