/**
 * @file ClockConduct.h
 * @brief RTC/NTP clock state management
 * @version 251231E
 * @date 2025-12-31
 *
 * Manages clock state and RTC synchronization. Provides interface for seeding
 * the system clock from RTC, syncing RTC from NTP time, and querying RTC
 * hardware availability.
 */

#pragma once

class PRTClock;

class ClockConduct {
public:
    void plan();

    static bool seedClockFromRtc(PRTClock &clock);
    static void syncRtcFromClock(const PRTClock &clock);
    static bool hasRtc();
};
