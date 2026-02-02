/**
 * @file CalendarRun.h
 * @brief Calendar state management
 * @version 251231E
 * @date 2025-12-31
 *
 * Manages calendar state and periodic updates. Handles calendar data
 * loading, state refresh scheduling, and calendar-triggered sentence playback.
 * Coordinates with CalendarPolicy for decision making.
 */

#pragma once

struct TodayState;

class CalendarRun {
public:
  void plan();
  static void cb_loadCalendar();
  static void cb_calendarSentence();

  bool todayReady() const;
  bool todayRead(TodayState& out) const;
};

extern CalendarRun calendarRun;
