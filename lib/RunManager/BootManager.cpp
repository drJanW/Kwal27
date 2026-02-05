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

BootManager bootManager;

namespace {

void cb_endOfBoot() {
    if (!AlertState::isBootPhase()) return;  // already ended
    PL("[Boot] Timeout - forcing START_RUNTIME");
    AlertRun::report(AlertRequest::START_RUNTIME);
}

} // namespace

bool BootManager::begin() {
    cancelFallbackTimer();
    fallback.resetFlags();
    if (!timers.create(Globals::clockBootstrapIntervalMs, 0, cb_bootstrapThunk)) {
        PL("[Run] BootManager failed to arm bootstrap timer");
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
    PF("[Boot] Timer restarted with bootPhaseMs=%u\n", Globals::bootPhaseMs);
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
                    PF("[Run] Clock tick started with NTP (%02u:%02u:%02u)\n",
                       prtClock.getHour(), prtClock.getMinute(), prtClock.getSecond());
                    AlertRun::report(AlertRequest::NTP_OK);
                } else if (wasFallback) {
                    PF("[Run] Clock tick promoted to NTP (%02u:%02u:%02u)\n",
                       prtClock.getHour(), prtClock.getMinute(), prtClock.getSecond());
                    AlertRun::report(AlertRequest::NTP_OK);
                }
            } else {
                PL("[Run] Failed to start clock tick with NTP");
            }
        }
        return;
    }

    bool isRunning = RunManager::isClockRunning();
    bool inFallback = RunManager::isClockInFallback();

    if (isRunning && inFallback) {
        if (!fallback.stateAnnounced) {
            fallback.stateAnnounced = true;
            PL("[Run] Modules running with fallback time");
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
            PL("[Run] Seeded clock from RTC snapshot");
        } else {
            // Ultimate fallback: 20 april 04:00 (from Globals)
            prtClock.setTime(Globals::fallbackHour, 0, 0);
            prtClock.setDay(Globals::fallbackDay);
            prtClock.setMonth(Globals::fallbackMonth);
            prtClock.setYear(Globals::fallbackYear - 2000);  // PRTClock uses 2-digit year
            fallback.seededFromCache = false;
            fallback.seededFromRtc = false;
            PF("[Run] No time source - using fallback: %02d/%02d/%04d %02d:00\n",
               Globals::fallbackDay, Globals::fallbackMonth, Globals::fallbackYear, Globals::fallbackHour);
        }
    }

    bool wasFallback = RunManager::isClockInFallback();
    if (RunManager::requestStartClockTick(true)) {
        fallback.stateAnnounced = false;
        if (!wasFallback) {
            if (fallback.seededFromRtc) {
                PL("[Run] Clock tick running in fallback path (RTC)");
            } else if (fallback.seededFromCache) {
                PL("[Run] Clock tick running in fallback path (seeded)");
            } else {
                PL("[Run] Clock tick running in fallback path");
            }
        }
    } else {
        PL("[Run] Failed to start clock tick in fallback path");
        fallback.seedAttempted = false;
        fallback.seededFromCache = false;
        timers.restart(Globals::ntpFallbackTimeoutMs, 1, cb_fallbackThunk);
    }
}
