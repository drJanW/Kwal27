/**
 * @file CalendarBoot.h
 * @brief Calendar subsystem one-time initialization
 * @version 251231E
 * @date 2025-12-31
 *
 * Handles calendar subsystem initialization during boot. Waits for SD card
 * and clock availability, then triggers calendar data loading. Part of the
 * Boot→Plan→Policy→Conduct pattern for calendar context coordination.
 */

#pragma once

class CalendarBoot {
public:
  void plan();
};

extern CalendarBoot calendarBoot;
