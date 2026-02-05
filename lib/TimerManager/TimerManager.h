/**
 * @file TimerManager.h
 * @brief Central non-blocking timer pool using callbacks (replaces scattered millis()/delay()).
 * @version 260201A
 * @date 2026-02-01
 */
#pragma once
#include <Arduino.h>

// Type alias for timer callbacks (plain function pointer).
typedef void (*TimerCallback)();

// Helper macro for callbacks declared inside classes/modules.
#define cb_type static void

class TimerManager {
public:
    /// @brief Default constructor. Prefer using global `timers` instance.
    TimerManager();

    struct Timer {
        bool active = false;           ///< Timer slot in use?
        TimerCallback cb = nullptr;    ///< Callback function pointer
        uint8_t token = 1;             ///< Identity token (allows multiple timers per callback)
        uint32_t interval = 0;         ///< Current interval in ms (may grow if growthFactor > 1.0)
        uint32_t nextTime = 0;         ///< Absolute millis() timestamp for next fire
        uint8_t repeat = 0;            ///< Remaining fires: 0=infinite, 1=last, >1=countdown
        float growthFactor = 1.0f;     ///< Interval multiplier per fire (1.0=constant, >1.0=backoff)
    };

    static const uint8_t MAX_TIMERS = 60;

    /**
     * @brief Create a timer.
     *
     * @param interval   Initial interval in milliseconds.
     * @param repeat     0 = infinite, 1 = one-shot, >1 = exact N fires total.
     * @param cb         Callback function.
     * @param growth     Interval multiplier per fire (1.0 = constant).
     * @param token      Timer identity token (default 1). Use different tokens for multiple timers with same callback.
     *
     * @return true on success, false if no slot is available or (cb, token) already in use.
     */
    bool create(uint32_t interval, uint8_t repeat, TimerCallback cb, float growth = 1.0f, uint8_t token = 1);

    /**
     * @brief Cancel a timer by (callback, token) identity.
     */
    void cancel(TimerCallback cb, uint8_t token = 1);

    /**
     * @brief Restart a timer: cancels existing timer (if any) and creates new one.
     *
     * Use restart() instead of create() when:
     * - Timer may or may not already exist
     * - Changing interval/repeat of an active timer
     * - Sequence callbacks that reuse the same function with different timings
     *
     * @return true on success, false if no slot available.
     */
    bool restart(uint32_t interval, uint8_t repeat, TimerCallback cb, float growth = 1.0f, uint8_t token = 1);

    /**
     * @brief Update all timers.
     * Must be called once per loop iteration.
     *
     * @note Callbacks are allowed to cancel or reconfigure timers.
     *       TimerManager detects such changes and will not override them.
     */
    void update();

    /**
     * @brief Check whether a timer with this (callback, token) identity is active.
     */
    bool isActive(TimerCallback cb, uint8_t token = 1) const;

    /**
     * @brief Get remaining repeat count.
     * @return 0 for infinite timers, >0 for remaining fires, -1 if not found.
     */
    int16_t getRepeatCount(TimerCallback cb, uint8_t token = 1) const;

    /**
     * @brief Get the number of active timers.
     */
    uint8_t getActiveCount() const;

    /**
     * @brief Diagnostics: report current and peak timer usage.
     * @param showAlways If true, always log; if false, only log when new peak reached.
     * @note Only outputs when SHOW_TIMER_STATUS is defined as true in Globals.h
     */
    void showAvailableTimers(bool showAlways);

private:
    Timer timers[MAX_TIMERS];
};

/// @brief Global TimerManager instance - preferred access method
/// @note Defined in TimerManager.cpp. Preferred access method.
extern TimerManager timers;