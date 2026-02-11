/**
 * @file CalendarRun.h
 * @brief Calendar state management
 * @version 260202A
 $12026-02-05
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
