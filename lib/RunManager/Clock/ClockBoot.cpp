/**
 * @file ClockBoot.cpp
 * @brief RTC/NTP clock one-time initialization implementation
 * @version 260205A
 * @date 2026-02-05
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
