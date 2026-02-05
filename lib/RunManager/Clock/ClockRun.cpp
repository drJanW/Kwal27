/**
 * @file ClockRun.cpp
 * @brief RTC/NTP clock state management implementation
 * @version 260204A
 * @date 2026-02-04
 */
#include <Arduino.h>
#include "ClockRun.h"

#include "ClockPolicy.h"
#include "Globals.h"
#include "PRTClock.h"

void ClockRun::plan() {
    if (ClockPolicy::isRtcAvailable()) {
        PL("[Run][Plan] RTC run ready (fallback + sync)");
    } else {
        PL("[Run][Plan] RTC hardware not detected");
    }
}

bool ClockRun::seedClockFromRtc(PRTClock &clock) {
    return ClockPolicy::seedClockFromRTC(clock);
}

void ClockRun::syncRtcFromClock(const PRTClock &clock) {
    ClockPolicy::syncRTCFromClock(clock);
}

bool ClockRun::hasRtc() {
    return ClockPolicy::isRtcAvailable();
}
