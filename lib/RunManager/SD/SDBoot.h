/**
 * @file SDBoot.h
 * @brief SD card one-time initialization
 * @version 260216G
 * @date 2026-02-16
 */
#pragma once

class SDBoot {
public:
    bool plan();
    
    /// Called by AlertRun when RTC_OK or NTP_OK is reported.
    /// Triggers deferred index rebuild if pending.
    static void onTimeAvailable();

    /// Request an index rebuild from outside (e.g. web handler).
    /// Schedules the rebuild in the Run layer via timer.
    static void requestRebuild();
    
private:
    static void cb_retryBoot();
};
