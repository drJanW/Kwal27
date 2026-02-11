/**
 * @file ClockRun.h
 * @brief RTC/NTP clock state management
 * @version 260131A
 $12026-02-05
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
