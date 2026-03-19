/**
 * @file ClockPolicy.h
 * @brief RTC/NTP clock business logic
 * @version 260212H
 * @date 2026-02-12
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
