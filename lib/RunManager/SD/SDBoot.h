/**
 * @file SDBoot.h
 * @brief SD card one-time initialization
 * @version 260131A
 $12026-02-05
 */
#pragma once

class SDBoot {
public:
    bool plan();
    
    /// Called by AlertRun when RTC_OK or NTP_OK is reported.
    /// Triggers deferred index rebuild if pending.
    static void onTimeAvailable();
    
private:
    static void cb_retryBoot();
};
