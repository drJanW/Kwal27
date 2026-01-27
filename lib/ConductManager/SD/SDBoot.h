/**
 * @file SDBoot.h
 * @brief SD card one-time initialization
 * @version 251231E
 * @date 2025-12-31
 *
 * Handles SD card initialization during boot with retry logic. Mounts the SD
 * card, verifies firmware version, and reports SD status to notification
 * system. Part of the Boot→Plan→Policy→Conduct pattern.
 * 
 * Index rebuild is deferred until valid time (RTC/NTP) is available,
 * triggered via onTimeAvailable() from NotifyConduct.
 */

#pragma once

class SDBoot {
public:
    bool plan();
    
    /// Called by NotifyConduct when RTC_OK or NTP_OK is reported.
    /// Triggers deferred index rebuild if pending.
    static void onTimeAvailable();
    
private:
    static void cb_retryBoot();
};
