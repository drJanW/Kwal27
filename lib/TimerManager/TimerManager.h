/**
 * @file TimerManager.h
 * @brief Non-blocking timer system using callbacks, replaces millis()/delay()
 * @version 0101F5A
 * @date 2026-01-01
 *
 * Provides a centralized timer management system for scheduling recurring and
 * one-shot callbacks without blocking. Supports fixed-interval repeating timers
 * and growing interval timers for retry scenarios. Eliminates the need for
 * scattered millis() checks and delay() calls throughout the codebase by
 * providing a clean callback-based API.
 */

// =============================================
// TimerManager.h
// =============================================
#pragma once
#include <Arduino.h>

// Type alias for timer callbacks
typedef void (*TimerCallback)();
#define cb_type static void

class TimerManager {
public:
    struct Timer {
        bool active = false;
        TimerCallback cb = nullptr;
        uint32_t interval = 0;   // ms (current interval, may grow if growthFactor > 1.0)
        uint32_t nextTime = 0;   // millis() when callback should run
        uint8_t repeat = 0;      // 0 = infinite, 1-255 = remaining repeats
        float growthFactor = 1.0f;  // interval multiplier per run (1.0 = constant)
    };

    static const uint8_t MAX_TIMERS = 60;

    static TimerManager& instance();

    // Create a timer
    // interval: ms (initial interval)
    // repeat:  0 = infinite, 1-255 = N repeats
    // cb: callback function
    // growth: interval multiplier per run (default 1.0 = constant, >1.0 = growing)
    // returns true if created, false if failed
    bool create(uint32_t interval, uint8_t repeat, TimerCallback cb, float growth = 1.0f);

    // Cancel a timer by callback
    void cancel(TimerCallback cb);

    // Restart (or start) a timer by callback
    bool restart(uint32_t interval, uint8_t repeat, TimerCallback cb, float growth = 1.0f);

    // Update timers (call in loop)
    void update();

    bool isActive(TimerCallback cb) const;

    // Get remaining repeat count for a timer (-1 if not found)
    int16_t getRepeatCount(TimerCallback cb) const;

    // Get count of active timers
    uint8_t getActiveCount() const;

    // Diagnostics
    void showAvailableTimers(bool showAlways);

private:
    TimerManager();

    Timer timers[MAX_TIMERS];
};
