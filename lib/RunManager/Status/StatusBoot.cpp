/**
 * @file StatusBoot.cpp
 * @brief Status display one-time initialization implementation
 * @version 251231E
 * @date 2025-12-31
 *
 * Implements status boot sequence: configures StatusPolicy and provides
 * time display callback that logs current time with source indication
 * (NTP or fallback).
 */

#include "StatusBoot.h"
#include "Globals.h"
#include "StatusPolicy.h"
#include "PRTClock.h"
#include "Alert/AlertRun.h"
#include "ContextController.h"

StatusBoot statusBoot;

void cb_timeDisplay() {

    bool clockSeeded = prtClock.isTimeFetched() || prtClock.getYear() != 0 || prtClock.getHour() != 0 || prtClock.getMinute() != 0;
    if (!clockSeeded) {
        return;  // Silent - no spam
    }

    ContextController::refreshTimeRead();
    const auto &timeState = ContextController::time();
    const char *source = prtClock.isTimeFetched() ? "ntp" : "fallback";
     PF("[Run] Time: %02u:%02u:%02u (%u-%02u-%02u, %s)\n",
         timeState.hour, timeState.minute, timeState.second,
         static_cast<unsigned>(timeState.year),
         timeState.month, timeState.day,
         source);
}

void StatusBoot::plan() {
    PL("[Run][Plan] Status boot sequencing");
    StatusPolicy::configure();
    AlertRun::plan();
}
