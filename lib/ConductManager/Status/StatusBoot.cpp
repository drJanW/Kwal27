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
#include "Notify/NotifyConduct.h"
#include "ContextManager.h"

StatusBoot statusBoot;

void cb_timeDisplay() {
    auto &clock = PRTClock::instance();
    bool clockSeeded = clock.isTimeFetched() || clock.getYear() != 0 || clock.getHour() != 0 || clock.getMinute() != 0;
    if (!clockSeeded) {
        return;  // Silent - no spam
    }

    ContextManager::refreshTimeRead();
    const auto &timeCtx = ContextManager::time();
    const char *source = clock.isTimeFetched() ? "ntp" : "fallback";
    PF("[Conduct] Time now: %02u:%02u:%02u (%u-%02u-%02u, %s)\n",
       timeCtx.hour, timeCtx.minute, timeCtx.second,
       static_cast<unsigned>(timeCtx.year),
       timeCtx.month, timeCtx.day,
       source);
}

void StatusBoot::plan() {
    PL("[Conduct][Plan] Status boot sequencing");
    StatusPolicy::configure();
    NotifyConduct::plan();
}
