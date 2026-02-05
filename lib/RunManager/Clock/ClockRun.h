/**
 * @file ClockRun.h
 * @brief RTC/NTP clock state management
 * @version 260131A
 * @date 2026-01-31
 */
#pragma once

class PRTClock;

class ClockRun {
public:
    void plan();

    static bool seedClockFromRtc(PRTClock &clock);
    static void syncRtcFromClock(const PRTClock &clock);
    static bool hasRtc();
};
