/**
 * @file CalendarRun.h
 * @brief Calendar state management
 * @version 260409K
 * @date 2026-04-09
 */
#pragma once

struct TodayState;

class CalendarRun {
public:
  void plan();
  static void cb_loadCalendar();
  static void cb_downloadCalendarSentence();
  static void cb_calendarSentence();

  bool todayReady() const;
  bool todayRead(TodayState& out) const;
};

extern CalendarRun calendarRun;
