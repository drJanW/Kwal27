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
#include "Notify/NotifyRun.h"
#include "ContextManager.h"

StatusBoot statusBoot;

void cb_timeDisplay() {

    bool clockSeeded = prtClock.isTimeFetched() || prtClock.getYear() != 0 || prtClock.getHour() != 0 || prtClock.getMinute() != 0;
    if (!clockSeeded) {
        return;  // Silent - no spam
    }

    ContextManager::refreshTimeRead();
    const auto &timeCtx = ContextManager::time();
    const char *source = prtClock.isTimeFetched() ? "ntp" : "fallback";
    PF("[Run] Time now: %02u:%02u:%02u (%u-%02u-%02u, %s)\\n",
       timeCtx.hour, timeCtx.minute, timeCtx.second,
       static_cast<unsigned>(timeCtx.year),
       timeCtx.month, timeCtx.day,
       source);
}

void StatusBoot::plan() {
    PL("[Run][Plan] Status boot sequencing");
    StatusPolicy::configure();
    NotifyRun::plan();
}
