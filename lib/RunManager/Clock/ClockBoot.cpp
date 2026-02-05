/**
 * @file ClockBoot.cpp
 * @brief RTC/NTP clock one-time initialization implementation
 * @version 260205C
 * @date 2026-02-05
 *
 * Implements clock boot sequence: initializes ClockPolicy and attempts to
 * seed the system clock from the DS3231 RTC module if available.
 */

#include "ClockBoot.h"

#include "ClockPolicy.h"
#include "PRTClock.h"
#include "Globals.h"

void ClockBoot::plan() {
    prtClock.begin();
    if (RTC_PRESENT) {
        ClockPolicy::begin();

        (void)ClockPolicy::seedClockFromRTC(prtClock);
    } else {
        PL("[ClockBoot] RTC absent per HWconfig");
    }
}
