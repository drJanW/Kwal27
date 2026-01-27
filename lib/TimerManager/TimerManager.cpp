/**
 * @file TimerManager.cpp
 * @brief Non-blocking timer system using callbacks, replaces millis()/delay()
 * @version 0101F5A
 * @date 2026-01-01
 *
 * Implementation of the TimerManager class. Manages a pool of timers that are
 * checked each loop iteration via update(). When a timer's interval elapses,
 * its callback is invoked. Supports infinite repeating timers, fixed-count timers,
 * and growing interval timers for retry logic. Enforces one-callback-per-timer
 * policy to prevent duplicate registrations and provides methods to create, cancel,
 * restart, and query timer status.
 */

// =============================================
// TimerManager.cpp
// =============================================
#include "TimerManager.h"
#include "Globals.h"   // for logging macros

// Use LOCAL_LOG_LEVEL to control verbosity per official architecture
// Set to LOG_LEVEL_DEBUG for verbose timer diagnostics
#ifndef LOCAL_LOG_LEVEL
#define LOCAL_LOG_LEVEL LOG_LEVEL_INFO
#endif

TimerManager& TimerManager::instance() {
    static TimerManager inst;
    return inst;
}

TimerManager::TimerManager() {
    for (uint8_t i = 0; i < MAX_TIMERS; i++) {
        timers[i].active = false;
        timers[i].growthFactor = 1.0f;
    }
}

bool TimerManager::create(uint32_t interval, uint8_t repeat, TimerCallback cb, float growth) {
    if (!cb) return false;

    // enforce "one callback = one timer"
    for (uint8_t i = 0; i < MAX_TIMERS; i++) {
        if (timers[i].active && timers[i].cb == cb) {
            LOG_DEBUG("[TimerManager] creation failed - callback already in use\n");
            return false;
        }
    }

    // find free timer
    for (uint8_t i = 0; i < MAX_TIMERS; i++) {
        if (!timers[i].active) {
            timers[i].active = true;
            timers[i].cb = cb;
            timers[i].interval = interval;
            timers[i].nextTime = millis() + interval;
            timers[i].repeat = repeat;
            // Force growth = 1.0f for infinite timers
            timers[i].growthFactor = (repeat == 0) ? 1.0f : growth;
            return true;
        }
    }

    LOG_WARN("[TimerManager] no free timers!\n");
    return false;
}

void TimerManager::cancel(TimerCallback cb) {
    if (!cb) return;
    for (uint8_t i = 0; i < MAX_TIMERS; i++) {
        if (timers[i].active && timers[i].cb == cb) {
            timers[i].active = false;
            timers[i].cb = nullptr;
            return;
        }
    }
}

bool TimerManager::restart(uint32_t interval, uint8_t repeat, TimerCallback cb, float growth) {
    if (cb) cancel(cb);
    return create(interval, repeat, cb, growth);
}

bool TimerManager::isActive(TimerCallback cb) const {
    if (!cb) {
        return false;
    }
    for (uint8_t i = 0; i < MAX_TIMERS; i++) {
        if (timers[i].active && timers[i].cb == cb) {
            return true;
        }
    }
    return false;
}

int16_t TimerManager::getRepeatCount(TimerCallback cb) const {
    if (!cb) return -1;
    for (uint8_t i = 0; i < MAX_TIMERS; i++) {
        if (timers[i].active && timers[i].cb == cb) {
            return timers[i].repeat;
        }
    }
    return -1;
}

void TimerManager::update() {
    uint32_t now = millis();
    for (uint8_t i = 0; i < MAX_TIMERS; i++) {
        if (!timers[i].active) continue;
        if ((int32_t)(now - timers[i].nextTime) >= 0) {
            // run callback
            TimerCallback cb = timers[i].cb;
            const uint8_t originalRepeat = timers[i].repeat;
            const uint32_t originalInterval = timers[i].interval;
            const uint32_t originalNextTime = timers[i].nextTime;
            const float originalGrowthFactor = timers[i].growthFactor;

            if (cb) cb();

            // if callback cancelled or altered this timer, leave it alone
            if (!timers[i].active) {
                continue;
            }
            if (timers[i].cb != cb) {
                continue;
            }
            if (timers[i].interval != originalInterval ||
                timers[i].nextTime != originalNextTime ||
                timers[i].repeat != originalRepeat ||
                timers[i].growthFactor != originalGrowthFactor) {
                continue;
            }

            // reschedule or finish using original parameters
            if (originalRepeat == 1) {
                // Last repeat - deactivate
                timers[i].active = false;
                timers[i].cb = nullptr;
                timers[i].growthFactor = 1.0f;
            } else if (originalRepeat > 1) {
                // More repeats - count down
                timers[i].repeat--;
                
                // Apply growth factor if > 1.0
                if (timers[i].growthFactor > 1.0f) {
                    uint32_t newInterval = (uint32_t)(timers[i].interval * timers[i].growthFactor);
                    timers[i].interval = min(newInterval, MAX_GROWING_INTERVAL_MS);
                }
                
                timers[i].nextTime += timers[i].interval;
            } else {
                // Infinite timer (repeat == 0)
                timers[i].nextTime += timers[i].interval;
            }
        }
    }
}

uint8_t TimerManager::getActiveCount() const {
    uint8_t count = 0;
    for (uint8_t i = 0; i < MAX_TIMERS; i++) {
        if (timers[i].active) count++;
    }
    return count;
}

// ===================================================
// Diagnostics
// ===================================================
void TimerManager::showAvailableTimers(bool showAlways) {
#if SHOW_TIMER_STATUS
    static uint8_t maxUsed = 0;
    uint8_t usedCount = 0;
    for (uint8_t i = 0; i < MAX_TIMERS; i++) {
        if (timers[i].active) usedCount++;
    }

    if (usedCount > maxUsed) {
        maxUsed = usedCount;
        PF("[TimerManager] New peak: %d/%d timers in use\n", usedCount, MAX_TIMERS);
    }

    if (showAlways) {
        PF("[TimerManager] Timers: %d/%d used (peak %d)\n", usedCount, MAX_TIMERS, maxUsed);
    }
#else
    (void)showAlways;  // Suppress unused parameter warning
#endif
}

