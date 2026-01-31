/**
 * @file CalendarRun.h
 * @brief Calendar context state management
 * @version 251231E
 * @date 2025-12-31
 *
 * Manages calendar context state and periodic updates. Handles calendar data
 * loading, context refresh scheduling, and calendar-triggered sentence playback.
 * Coordinates with CalendarPolicy for decision making.
 */

#pragma once

struct TodayContext;

class CalendarRun {
public:
  void plan();
  static void cb_loadCalendar();
  static void cb_calendarSentence();

  bool contextReady() const;
  bool contextRead(TodayContext& out) const;
};

extern CalendarRun calendarRun;
