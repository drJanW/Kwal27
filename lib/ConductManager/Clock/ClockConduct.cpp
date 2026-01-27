/**
 * @file ClockConduct.cpp
 * @brief RTC/NTP clock state management implementation
 * @version 251231E
 * @date 2025-12-31
 *
 * Implements clock state management: delegates RTC operations to ClockPolicy,
 * provides unified interface for clock seeding and synchronization operations.
 */

#include "ClockConduct.h"

#include "ClockPolicy.h"
#include "PRTClock.h"
#include "Globals.h"

void ClockConduct::plan() {
    if (ClockPolicy::isRtcAvailable()) {
        PL("[Conduct][Plan] RTC conduct ready (fallback + sync)");
    } else {
        PL("[Conduct][Plan] RTC hardware not detected");
    }
}

bool ClockConduct::seedClockFromRtc(PRTClock &clock) {
    return ClockPolicy::seedClockFromRTC(clock);
}

void ClockConduct::syncRtcFromClock(const PRTClock &clock) {
    ClockPolicy::syncRTCFromClock(clock);
}

bool ClockConduct::hasRtc() {
    return ClockPolicy::isRtcAvailable();
}
