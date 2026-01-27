/**
 * @file ClockBoot.cpp
 * @brief RTC/NTP clock one-time initialization implementation
 * @version 251231E
 * @date 2025-12-31
 *
 * Implements clock boot sequence: initializes ClockPolicy and attempts to
 * seed the system clock from the DS3231 RTC module if available.
 */

#include "ClockBoot.h"

#include "ClockPolicy.h"
#include "PRTClock.h"
#include "Globals.h"

void ClockBoot::plan() {
    if (RTC_PRESENT) {
        ClockPolicy::begin();
        auto &clock = PRTClock::instance();
        (void)ClockPolicy::seedClockFromRTC(clock);
    } else {
        PL("[ClockBoot] RTC absent per HWconfig");
    }
}
