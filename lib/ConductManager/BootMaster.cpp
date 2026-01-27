/**
 * @file BootMaster.cpp
 * @brief Master boot sequence orchestrator implementation
 * @version 251231E
 * @date 2025-12-31
 *
 * Implements the boot sequence orchestration logic. Coordinates the startup
 * of all subsystems: SD card, WiFi, clock, sensors, audio, lights, and web
 * interface. Handles fallback scenarios when components fail to initialize
 * within expected timeframes.
 */

#include "BootMaster.h"
#include "Globals.h"
#include "TimerManager.h"
#include "PRTClock.h"
#include "ConductManager.h"
#include "Notify/NotifyConduct.h"
#include "Notify/NotifyState.h"

BootMaster bootMaster;

namespace {

TimerManager &timers() {
    return TimerManager::instance();
}

void cb_endOfBoot() {
    if (!NotifyState::isBootPhase()) return;  // already ended
    PL("[Boot] Timeout - forcing START_RUNTIME");
    NotifyConduct::report(NotifyIntent::START_RUNTIME);
}

} // namespace

bool BootMaster::begin() {
    cancelFallbackTimer();
    fallback.resetFlags();
    if (!timers().create(Globals::clockBootstrapIntervalMs, 0, cb_bootstrapThunk)) {
        PL("[Conduct] BootMaster failed to arm bootstrap timer");
        return false;
    }
    // Boot timeout: force START_RUNTIME after bootPhaseMs regardless of clock
    // Uses code default; restartBootTimer() updates after globals.csv load
    timers().create(Globals::bootPhaseMs, 1, cb_endOfBoot);
    return true;
}

void BootMaster::restartBootTimer() {
    if (!NotifyState::isBootPhase()) return;  // already ended
    timers().cancel(cb_endOfBoot);
    timers().create(Globals::bootPhaseMs, 1, cb_endOfBoot);
    PF("[Boot] Timer restarted with bootPhaseMs=%u\n", Globals::bootPhaseMs);
}

void BootMaster::cb_bootstrapThunk() {
    bootMaster.cb_bootstrap();
}

void BootMaster::cb_bootstrap() {
    auto &clock = PRTClock::instance();

    if (PRTClock::instance().isTimeFetched()) {
        cancelFallbackTimer();
        fallback.resetFlags();

        bool wasRunning = ConductManager::isClockRunning();
        bool wasFallback = ConductManager::isClockInFallback();
            if (!wasRunning || wasFallback) {
            if (ConductManager::intentStartClockTick(false)) {
                if (!wasRunning) {
                    PF("[Conduct] Clock tick started with NTP (%02u:%02u:%02u)\n",
                       clock.getHour(), clock.getMinute(), clock.getSecond());
                    NotifyConduct::report(NotifyIntent::NTP_OK);
                } else if (wasFallback) {
                    PF("[Conduct] Clock tick promoted to NTP (%02u:%02u:%02u)\n",
                       clock.getHour(), clock.getMinute(), clock.getSecond());
                    NotifyConduct::report(NotifyIntent::NTP_OK);
                }
            } else {
                PL("[Conduct] Failed to start clock tick with NTP");
            }
        }
        return;
    }

    bool isRunning = ConductManager::isClockRunning();
    bool inFallback = ConductManager::isClockInFallback();

    if (isRunning && inFallback) {
        if (!fallback.stateAnnounced) {
            fallback.stateAnnounced = true;
            PL("[Conduct] Modules running with fallback time");
        }
        cancelFallbackTimer();
        return;
    }

    timers().restart(Globals::ntpFallbackTimeoutMs, 1, cb_fallbackThunk);
}

void BootMaster::cb_fallbackThunk() {
    bootMaster.fallbackTimeout();
}

void BootMaster::cancelFallbackTimer() {
    timers().cancel(cb_fallbackThunk);
}

void BootMaster::fallbackTimeout() {
    if (PRTClock::instance().isTimeFetched()) {
        fallback.resetFlags();
        return;
    }

    auto &clock = PRTClock::instance();

    if (!fallback.seedAttempted) {
        fallback.seedAttempted = true;
        if (ConductManager::intentSeedClockFromRtc()) {
            fallback.seededFromRtc = true;
            fallback.seededFromCache = false;
            PL("[Conduct] Seeded clock from RTC snapshot");
        } else {
            // Ultimate fallback: 20 april 04:00 (from Globals)
            clock.setTime(Globals::fallbackHour, 0, 0);
            clock.setDay(Globals::fallbackDay);
            clock.setMonth(Globals::fallbackMonth);
            clock.setYear(Globals::fallbackYear - 2000);  // PRTClock uses 2-digit year
            fallback.seededFromCache = false;
            fallback.seededFromRtc = false;
            PF("[Conduct] No time source - using fallback: %02d/%02d/%04d %02d:00\n",
               Globals::fallbackDay, Globals::fallbackMonth, Globals::fallbackYear, Globals::fallbackHour);
        }
    }

    bool wasFallback = ConductManager::isClockInFallback();
    if (ConductManager::intentStartClockTick(true)) {
        fallback.stateAnnounced = false;
        if (!wasFallback) {
            if (fallback.seededFromRtc) {
                PL("[Conduct] Clock tick running in fallback mode (RTC)");
            } else if (fallback.seededFromCache) {
                PL("[Conduct] Clock tick running in fallback mode (seeded)");
            } else {
                PL("[Conduct] Clock tick running in fallback mode");
            }
        }
    } else {
        PL("[Conduct] Failed to start clock tick in fallback mode");
        fallback.seedAttempted = false;
        fallback.seededFromCache = false;
        timers().restart(Globals::ntpFallbackTimeoutMs, 1, cb_fallbackThunk);
    }
}
