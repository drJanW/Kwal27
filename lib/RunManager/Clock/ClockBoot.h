/**
 * @file ClockBoot.h
 * @brief RTC/NTP clock one-time initialization
 * @version 251231E
 * @date 2025-12-31
 *
 * Handles clock subsystem initialization during boot. Configures the RTC
 * hardware and attempts initial time seeding from the DS3231 RTC module.
 * Part of the Boot→Plan→Policy→Run pattern for clock coordination.
 */

#pragma once

class ClockBoot {
public:
    void plan();
};
