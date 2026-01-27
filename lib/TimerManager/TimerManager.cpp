// =============================================
// TimerManager.cpp
// =============================================
/**
 * @file TimerManager.cpp
 * @brief Non-blocking timer system implementation
 * @version 260127A
 * @date 2026-01-27
 *
 * Manages a pool of 60 software timers checked each loop() iteration.
 * When a timer's interval elapses, its callback is invoked.
 *
 * Features:
 * - Infinite, one-shot, and counted timers
 * - Growing interval for retry/backoff patterns
 * - Callback reentrancy (callbacks may modify their own timer)
 * - (callback, token) identity allows multiple timers per callback
 *
 * @see TimerManager.h for API documentation and usage contract
 */

#include "TimerManager.h"
#include "Globals.h"   // for logging macros

// Use LOCAL_LOG_LEVEL to control verbosity per official architecture
// Set to LOG_LEVEL_DEBUG for verbose timer diagnostics
#ifndef LOCAL_LOG_LEVEL
#define LOCAL_LOG_LEVEL LOG_LEVEL_INFO
#endif

/// @brief Global timer manager instance - preferred access method
TimerManager timers;

/// @brief [DEPRECATED] Returns reference to global `timers` for backward compatibility
TimerManager& TimerManager::instance() {
    return ::timers;  // explicit global namespace
}

/// @brief Constructor - Timer struct defaults handle initialization
TimerManager::TimerManager() {
    // All Timer fields have in-class defaults (C++11)
    // No explicit initialization needed
}

bool TimerManager::create(uint32_t interval, uint8_t repeat, TimerCallback cb, float growth, uint8_t token) {
    if (!cb) return false;

    // Check for duplicate: same (callback, token) pair cannot exist twice
    for (uint8_t i = 0; i < MAX_TIMERS; i++) {
        if (timers[i].active && timers[i].cb == cb && timers[i].token == token) {
            LOG_DEBUG("[TimerManager] creation failed - (cb, token) already in use\n");
            return false;
        }
    }

    // Find first free slot
    for (uint8_t i = 0; i < MAX_TIMERS; i++) {
        if (!timers[i].active) {
            timers[i].active = true;
            timers[i].cb = cb;
            timers[i].token = token;
            timers[i].interval = interval;
            timers[i].nextTime = millis() + interval;
            timers[i].repeat = repeat;
            // Growth allowed for all timers; interval capped at MAX_GROWTH_INTERVAL_MS in update()
            timers[i].growthFactor = growth;
            return true;
        }
    }

    LOG_WARN("[TimerManager] no free timers!\n");
    return false;
}

/// @brief Deactivate timer and reset slot to defaults
void TimerManager::cancel(TimerCallback cb, uint8_t token) {
    if (!cb) return;
    for (uint8_t i = 0; i < MAX_TIMERS; i++) {
        if (timers[i].active && timers[i].cb == cb && timers[i].token == token) {
            timers[i].active = false;
            timers[i].cb = nullptr;
            timers[i].token = 1;
            timers[i].growthFactor = 1.0f;
            return;
        }
    }
}

/// @brief Cancel then create - always succeeds if slots available
bool TimerManager::restart(uint32_t interval, uint8_t repeat, TimerCallback cb, float growth, uint8_t token) {
    cancel(cb, token);  // Safe even if timer doesn't exist
    return create(interval, repeat, cb, growth, token);
}

bool TimerManager::isActive(TimerCallback cb, uint8_t token) const {
    if (!cb) {
        return false;
    }
    for (uint8_t i = 0; i < MAX_TIMERS; i++) {
        if (timers[i].active && timers[i].cb == cb && timers[i].token == token) {
            return true;
        }
    }
    return false;
}

int16_t TimerManager::getRepeatCount(TimerCallback cb, uint8_t token) const {
    if (!cb) return -1;
    for (uint8_t i = 0; i < MAX_TIMERS; i++) {
        if (timers[i].active && timers[i].cb == cb && timers[i].token == token) {
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

            // Buffer original values to detect if callback modified its own timer
            // (callbacks may call cancel/restart/create on themselves)
            const uint8_t originalRepeat = timers[i].repeat;
            const uint32_t originalInterval = timers[i].interval;
            const uint32_t originalNextTime = timers[i].nextTime;
            const float originalGrowthFactor = timers[i].growthFactor;
            const uint8_t originalToken = timers[i].token;

            // Execute callback (may modify this timer via cancel/restart)
            if (cb) cb();

            // Reentrancy detection: if callback modified this timer, respect its changes
            if (!timers[i].active) {
                continue;  // Callback cancelled itself
            }
            if (timers[i].cb != cb) {
                continue;  // Callback replaced itself with different callback
            }
            if (timers[i].token != originalToken) {
                continue;  // Callback changed token (reused slot)
            }
            // If any parameter changed, callback called restart() with new values
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
                timers[i].token = 1;
            } else {
                // Continuing timer: finite (repeat > 1) or infinite (repeat == 0)
                if (originalRepeat > 1) {
                    timers[i].repeat--;
                }
                // Apply growth factor if > 1.0
                if (timers[i].growthFactor > 1.0f) {
                    uint32_t newInterval = (uint32_t)(timers[i].interval * timers[i].growthFactor);
                    timers[i].interval = min(newInterval, MAX_GROWTH_INTERVAL_MS);
                }
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
    uint8_t usedCount = getActiveCount();

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