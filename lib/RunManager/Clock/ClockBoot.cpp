/**
 * @file ClockBoot.cpp
 * @brief RTC/NTP clock one-time initialization implementation
 * @version 260215B
 * @date 2026-02-15
 */
#include "ClockBoot.h"

#include "ClockPolicy.h"
#include "PRTClock.h"
#include "Globals.h"

void ClockBoot::plan() {
    // RTC already read in systemBootStage1() (time available before SD/WiFi)
    // Start I2CInitHelper for ongoing RTC health tracking
    if (Globals::rtcPresent) {
        ClockPolicy::begin();
    }
}
