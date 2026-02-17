/**
 * @file SDBoot.h
 * @brief SD card one-time initialization
 * @version 260217D
 * @date 2026-02-17
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

    /// Request a vote-preserving reindex of one dir.
    /// Schedules via timer so SD I/O runs outside web handler.
    static void requestSyncDir(uint8_t dirNum);
    
private:
    static void cb_retryBoot();
};
