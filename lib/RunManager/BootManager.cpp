/**
 * @file BootManager.cpp
 * @brief Boot sequence coordinator implementation
 * @version 260205A
 * @date 2026-02-05
 */
#include "BootManager.h"
#include "Globals.h"
#include "TimerManager.h"
#include "PRTClock.h"
#include "RunManager.h"
#include "Alert/AlertRun.h"
#include "Alert/AlertState.h"
#include "Sensors/SensorsRun.h"

BootManager bootManager;

namespace {

void cb_endOfBoot() {
    if (!AlertState::isBootPhase()) return;  // already ended
    SensorsRun::readRtcTemperature();
    PL("[BootManager] Ready");
    AlertRun::report(AlertRequest::START_RUNTIME);
}

} // namespace

bool BootManager::begin() {
    cancelFallbackTimer();
    fallback.resetFlags();
    if (!timers.create(Globals::clockBootstrapIntervalMs, 0, cb_bootstrapThunk)) {
        PL("[BootManager] Failed to arm bootstrap timer");
        return false;
    }
    // Boot timeout: force START_RUNTIME after bootPhaseMs regardless of clock
    // Uses code default; restartBootTimer() updates after globals.csv load
    timers.create(Globals::bootPhaseMs, 1, cb_endOfBoot);
    return true;
}

void BootManager::restartBootTimer() {
    if (!AlertState::isBootPhase()) return;  // already ended
    timers.cancel(cb_endOfBoot);
    timers.create(Globals::bootPhaseMs, 1, cb_endOfBoot);
    PF_BOOT("[BootManager] bootPhaseMs=%u\n", Globals::bootPhaseMs);
}

void BootManager::cb_bootstrapThunk() {
    bootManager.cb_bootstrap();
}

void BootManager::cb_bootstrap() {
    if (prtClock.isTimeFetched()) {
        cancelFallbackTimer();
        fallback.resetFlags();

        bool wasRunning = RunManager::isClockRunning();
        bool wasFallback = RunManager::isClockInFallback();
            if (!wasRunning || wasFallback) {
            if (RunManager::requestStartClockTick(false)) {
                if (!wasRunning) {
                    PL("[Clock] NTP ready");
                    AlertRun::report(AlertRequest::NTP_OK);
                } else if (wasFallback) {
                    PF_BOOT("[Clock] promoted to NTP\n");
                    AlertRun::report(AlertRequest::NTP_OK);
                }
            } else {
                PL("[Clock] Failed to start tick");
            }
        }
        return;
    }

    bool isRunning = RunManager::isClockRunning();
    bool inFallback = RunManager::isClockInFallback();

    if (isRunning && inFallback) {
        if (!fallback.stateAnnounced) {
            fallback.stateAnnounced = true;
            PL("[Clock] fallback time active");
        }
        cancelFallbackTimer();
        return;
    }

    timers.restart(Globals::ntpFallbackTimeoutMs, 1, cb_fallbackThunk);
}

void BootManager::cb_fallbackThunk() {
    bootManager.fallbackTimeout();
}

void BootManager::cancelFallbackTimer() {
    timers.cancel(cb_fallbackThunk);
}

void BootManager::fallbackTimeout() {
    if (prtClock.isTimeFetched()) {
        fallback.resetFlags();
        return;
    }

    if (!fallback.seedAttempted) {
        fallback.seedAttempted = true;
        if (RunManager::requestSeedClockFromRtc()) {
            fallback.seededFromRtc = true;
            fallback.seededFromCache = false;
            PL("[Clock] seeded from RTC");
        } else {
            // Ultimate fallback: 20 april 04:00 (from Globals)
            prtClock.setTime(Globals::fallbackHour, 0, 0);
            prtClock.setDay(Globals::fallbackDay);
            prtClock.setMonth(Globals::fallbackMonth);
            prtClock.setYear(Globals::fallbackYear - 2000);  // PRTClock uses 2-digit year
            fallback.seededFromCache = false;
            fallback.seededFromRtc = false;
            PF("[Clock] fallback date %02d/%02d/%04d %02d:00\n",
               Globals::fallbackDay, Globals::fallbackMonth, Globals::fallbackYear, Globals::fallbackHour);
        }
    }

    bool wasFallback = RunManager::isClockInFallback();
    if (RunManager::requestStartClockTick(true)) {
        fallback.stateAnnounced = false;
        if (!wasFallback) {
            if (fallback.seededFromRtc) {
                PL("[Clock] fallback tick (rtc)");
            } else if (fallback.seededFromCache) {
                PL("[Clock] fallback tick (seeded)");
            } else {
                PL("[Clock] fallback tick (default)");
            }
        }
    } else {
        PL("[Clock] failed to start fallback tick");
        fallback.seedAttempted = false;
        fallback.seededFromCache = false;
        timers.restart(Globals::ntpFallbackTimeoutMs, 1, cb_fallbackThunk);
    }
}
