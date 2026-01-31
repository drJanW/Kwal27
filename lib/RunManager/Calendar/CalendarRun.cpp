/**
 * @file CalendarRun.cpp
 * @brief Calendar context state management implementation
 * @version 251231E
 * @date 2025-12-31
 *
 * Implements calendar state management: loads calendar CSV from SD card,
 * schedules periodic calendar sentence announcements, and coordinates
 * context updates with the light and audio subsystems.
 */

#include "CalendarRun.h"

#include "CalendarPolicy.h"
#include "Calendar.h"
#include "RunManager.h"
#include "Globals.h"
#include "TimerManager.h"
#include "PRTClock.h"
#include "SDManager.h"
#include "TodayContext.h"
#include "Light/LightRun.h"
#include "Notify/NotifyState.h"

namespace {

constexpr uint32_t retryStartMs      = 2UL * 1000UL;   // Start retry interval (grows)
constexpr int32_t  retryCount        = -50;            // 50 retries with growing interval
constexpr uint32_t initialDelayMs    = 5UL * 1000UL;

bool clockReady() {
  return prtClock.hasValidDate();
}

struct CalendarRunLogFlags {
  bool managerNotReady = false;
  bool sdBusy = false;
};

CalendarRunLogFlags logFlags;
bool initialDelayPending = true;

TodayContext todayContext;
bool todayContextValid = false;

void resetLogFlags() {
  logFlags.managerNotReady = false;
  logFlags.sdBusy = false;
}

void clearTodayContextRead() {
  todayContext = TodayContext{};
  todayContextValid = false;
}

void refreshTodayContextRead() {
  TodayContext ctx;
  if (LoadTodayContext(ctx) && ctx.valid) {
    todayContext = ctx;
    todayContextValid = true;
  } else {
    clearTodayContextRead();
  }
}

String sentence;
uint32_t sentenceIntervalMs = 0;

void clearSentenceTimer() {
  timers.cancel(CalendarRun::cb_calendarSentence);
  sentence = "";
  sentenceIntervalMs = 0;
}

bool getValidDate(uint16_t& year, uint8_t& month, uint8_t& day) {
  const uint16_t rawYear = prtClock.getYear();
  if (rawYear == 0) {
    return false;
  }
  year = rawYear >= 2000 ? rawYear : static_cast<uint16_t>(2000 + rawYear);
  month = prtClock.getMonth();
  day = prtClock.getDay();
  if (month == 0 || day == 0) {
    return false;
  }
  return true;
}

} // namespace

CalendarRun calendarRun;

void CalendarRun::plan() {
  timers.cancel(CalendarRun::cb_loadCalendar);
  clearSentenceTimer();

  if (!calendarManager.isReady()) {
    if (!logFlags.managerNotReady) {
      PF("[CalendarRun] Calendar manager not ready, retrying\n");
      logFlags.managerNotReady = true;
    }
    timers.restart(retryStartMs, retryCount, CalendarRun::cb_loadCalendar);
    return;
  }
  logFlags.managerNotReady = false;

  if (!clockReady()) {
    timers.restart(retryStartMs, retryCount, CalendarRun::cb_loadCalendar);
    return;
  }

  PF("[CalendarRun] Calendar scheduling enabled\n");

  if (initialDelayPending) {
    if (!timers.create(initialDelayMs, 1, CalendarRun::cb_loadCalendar)) {
      PF("[CalendarRun] Failed to arm initial calendar delay\n");
    } else {
      initialDelayPending = false;
    }
    return;
  }

  CalendarRun::cb_loadCalendar();
}

void CalendarRun::cb_loadCalendar() {
  if (!calendarManager.isReady()) {
    if (!logFlags.managerNotReady) {
      PF("[CalendarRun] Calendar manager not ready, retrying\n");
      logFlags.managerNotReady = true;
    }
    timers.restart(retryStartMs, retryCount, CalendarRun::cb_loadCalendar);
    return;
  }
  logFlags.managerNotReady = false;

  if (!clockReady()) {
    timers.restart(retryStartMs, retryCount, CalendarRun::cb_loadCalendar);
    return;
  }

  uint16_t year = 0;
  uint8_t month = 0;
  uint8_t day = 0;
  if (!getValidDate(year, month, day)) {
    timers.restart(retryStartMs, retryCount, CalendarRun::cb_loadCalendar);
    return;
  }

  bool calendarLoaded = false;
  {
    if (NotifyState::isSdBusy()) {
      if (!logFlags.sdBusy) {
        PF("[CalendarRun] SD busy, retrying\n");
        logFlags.sdBusy = true;
      }
      timers.restart(retryStartMs, retryCount, CalendarRun::cb_loadCalendar);
      return;
    }
    logFlags.sdBusy = false;
    SDManager::lockSD();
    calendarLoaded = calendarManager.loadToday(year, month, day);
    SDManager::unlockSD();
  }

  if (!calendarLoaded) {
    clearSentenceTimer();
    CalendarPolicy::applyThemeBox(CalendarThemeBox{});
    LightRun::applyPattern(0);
    LightRun::applyColor(0);
    clearTodayContextRead();
    NotifyState::setCalendarStatus(true);  // OK - geen bijzondere dag
    RunManager::triggerBootFragment();  // Theme box set, play first fragment
    PL("[CalendarRun] No calendar data for today");
    timers.restart(Globals::calendarRefreshIntervalMs, 0, CalendarRun::cb_loadCalendar);
    return;
  }

  const auto& calData = calendarManager.calendarData();
  CalendarPolicy::Decision decision;
  if (!CalendarPolicy::evaluate(calData, decision)) {
    clearSentenceTimer();
    CalendarPolicy::applyThemeBox(CalendarThemeBox{});
    LightRun::applyPattern(0);
    LightRun::applyColor(0);
    clearTodayContextRead();
    timers.restart(Globals::calendarRefreshIntervalMs, 0, CalendarRun::cb_loadCalendar);
    return;
  }

  if (decision.hasSentence) {
    sentence = calData.day.ttsSentence;
    sentenceIntervalMs = decision.sentenceIntervalMs;

    if (sentenceIntervalMs > 0) {
      // Use restart() - calendar can reload, replacing previous sentence timer
      if (!timers.restart(sentenceIntervalMs, 0, CalendarRun::cb_calendarSentence)) {
        PF("[CalendarRun] Failed to start calendar sentence timer (%lu ms)\n",
           static_cast<unsigned long>(sentenceIntervalMs));
      } else {
        // timer started successfully
      }
    } else {
      clearSentenceTimer();
    }

    CalendarPolicy::speakSentence(calData.day.ttsSentence);
  } else {
    clearSentenceTimer();
  }

  if (decision.hasThemeBox) {
    CalendarPolicy::applyThemeBox(calData.theme);
  } else {
    CalendarPolicy::applyThemeBox(CalendarThemeBox{});
  }

  // Apply calendar-driven pattern/color via LightRun intents
  LightRun::applyPattern(calData.day.patternId);
  LightRun::applyColor(calData.day.colorId);

  refreshTodayContextRead();
  NotifyState::setCalendarStatus(true);
  RunManager::triggerBootFragment();  // Theme box set, play first fragment
  PL("[CalendarRun] Calendar loaded");
  timers.restart(Globals::calendarRefreshIntervalMs, 0, CalendarRun::cb_loadCalendar);
  resetLogFlags();
}

void CalendarRun::cb_calendarSentence() {
  if (sentence.isEmpty()) {
    return;
  }
  CalendarPolicy::speakSentence(sentence);
}

bool CalendarRun::contextReady() const {
  return todayContextValid && todayContext.valid;
}

bool CalendarRun::contextRead(TodayContext& out) const {
  if (!contextReady()) {
    return false;
  }
  out = todayContext;
  return true;
}
