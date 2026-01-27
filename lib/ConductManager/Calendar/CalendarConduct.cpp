/**
 * @file CalendarConduct.cpp
 * @brief Calendar context state management implementation
 * @version 251231E
 * @date 2025-12-31
 *
 * Implements calendar state management: loads calendar CSV from SD card,
 * schedules periodic calendar sentence announcements, and coordinates
 * context updates with the light and audio subsystems.
 */

#include "CalendarConduct.h"

#include "CalendarPolicy.h"
#include "Calendar.h"
#include "ConductManager.h"
#include "Globals.h"
#include "TimerManager.h"
#include "PRTClock.h"
#include "SDManager.h"
#include "TodayContext.h"
#include "Light/LightConduct.h"
#include "Notify/NotifyState.h"

namespace {

constexpr uint32_t retryStartMs      = 2UL * 1000UL;   // Start retry interval (grows)
constexpr int32_t  retryCount        = -50;            // 50 retries with growing interval
constexpr uint32_t initialDelayMs    = 5UL * 1000UL;

TimerManager& timers() {
  return TimerManager::instance();
}

bool clockReady() {
  return PRTClock::instance().hasValidDate();
}

struct CalendarConductLogFlags {
  bool managerNotReady = false;
  bool sdBusy = false;
};

CalendarConductLogFlags logFlags;
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
  timers().cancel(CalendarConduct::cb_calendarSentence);
  sentence = "";
  sentenceIntervalMs = 0;
}

bool getValidDate(uint16_t& year, uint8_t& month, uint8_t& day) {
  auto& clock = PRTClock::instance();
  const uint16_t rawYear = clock.getYear();
  if (rawYear == 0) {
    return false;
  }
  year = rawYear >= 2000 ? rawYear : static_cast<uint16_t>(2000 + rawYear);
  month = clock.getMonth();
  day = clock.getDay();
  if (month == 0 || day == 0) {
    return false;
  }
  return true;
}

} // namespace

CalendarConduct calendarConduct;

void CalendarConduct::plan() {
  timers().cancel(CalendarConduct::cb_loadCalendar);
  clearSentenceTimer();

  if (!calendarManager.isReady()) {
    if (!logFlags.managerNotReady) {
      PF("[CalendarConduct] Calendar manager not ready, retrying\n");
      logFlags.managerNotReady = true;
    }
    timers().restart(retryStartMs, retryCount, CalendarConduct::cb_loadCalendar);
    return;
  }
  logFlags.managerNotReady = false;

  if (!clockReady()) {
    timers().restart(retryStartMs, retryCount, CalendarConduct::cb_loadCalendar);
    return;
  }

  PF("[CalendarConduct] Calendar scheduling enabled\n");

  if (initialDelayPending) {
    if (!timers().create(initialDelayMs, 1, CalendarConduct::cb_loadCalendar)) {
      PF("[CalendarConduct] Failed to arm initial calendar delay\n");
    } else {
      initialDelayPending = false;
    }
    return;
  }

  CalendarConduct::cb_loadCalendar();
}

void CalendarConduct::cb_loadCalendar() {
  if (!calendarManager.isReady()) {
    if (!logFlags.managerNotReady) {
      PF("[CalendarConduct] Calendar manager not ready, retrying\n");
      logFlags.managerNotReady = true;
    }
    timers().restart(retryStartMs, retryCount, CalendarConduct::cb_loadCalendar);
    return;
  }
  logFlags.managerNotReady = false;

  if (!clockReady()) {
    timers().restart(retryStartMs, retryCount, CalendarConduct::cb_loadCalendar);
    return;
  }

  uint16_t year = 0;
  uint8_t month = 0;
  uint8_t day = 0;
  if (!getValidDate(year, month, day)) {
    timers().restart(retryStartMs, retryCount, CalendarConduct::cb_loadCalendar);
    return;
  }

  bool calendarLoaded = false;
  {
    if (SDManager::isSDbusy()) {
      if (!logFlags.sdBusy) {
        PF("[CalendarConduct] SD busy, retrying\n");
        logFlags.sdBusy = true;
      }
      timers().restart(retryStartMs, retryCount, CalendarConduct::cb_loadCalendar);
      return;
    }
    logFlags.sdBusy = false;
    SDManager::setSDbusy(true);
    calendarLoaded = calendarManager.loadToday(year, month, day);
    SDManager::setSDbusy(false);
  }

  if (!calendarLoaded) {
    clearSentenceTimer();
    CalendarPolicy::applyThemeBox(CalendarThemeBox{});
    LightConduct::applyPattern(0);
    LightConduct::applyColor(0);
    clearTodayContextRead();
    NotifyState::setCalendarStatus(true);  // OK - geen bijzondere dag
    ConductManager::triggerBootFragment();  // Theme box set, play first fragment
    PL("[CalendarConduct] No calendar data for today");
    timers().restart(Globals::calendarRefreshIntervalMs, 0, CalendarConduct::cb_loadCalendar);
    return;
  }

  const auto& calData = calendarManager.calendarData();
  CalendarPolicy::Decision decision;
  if (!CalendarPolicy::evaluate(calData, decision)) {
    clearSentenceTimer();
    CalendarPolicy::applyThemeBox(CalendarThemeBox{});
    LightConduct::applyPattern(0);
    LightConduct::applyColor(0);
    clearTodayContextRead();
    timers().restart(Globals::calendarRefreshIntervalMs, 0, CalendarConduct::cb_loadCalendar);
    return;
  }

  if (decision.hasSentence) {
    sentence = calData.day.ttsSentence;
    sentenceIntervalMs = decision.sentenceIntervalMs;

    if (sentenceIntervalMs > 0) {
      // Use restart() - calendar can reload, replacing previous sentence timer
      if (!timers().restart(sentenceIntervalMs, 0, CalendarConduct::cb_calendarSentence)) {
        PF("[CalendarConduct] Failed to start calendar sentence timer (%lu ms)\n",
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

  // Apply calendar-driven pattern/color via LightConduct intents
  LightConduct::applyPattern(calData.day.patternId);
  LightConduct::applyColor(calData.day.colorId);

  refreshTodayContextRead();
  NotifyState::setCalendarStatus(true);
  ConductManager::triggerBootFragment();  // Theme box set, play first fragment
  PL("[CalendarConduct] Calendar loaded");
  timers().restart(Globals::calendarRefreshIntervalMs, 0, CalendarConduct::cb_loadCalendar);
  resetLogFlags();
}

void CalendarConduct::cb_calendarSentence() {
  if (sentence.isEmpty()) {
    return;
  }
  CalendarPolicy::speakSentence(sentence);
}

bool CalendarConduct::contextReady() const {
  return todayContextValid && todayContext.valid;
}

bool CalendarConduct::contextRead(TodayContext& out) const {
  if (!contextReady()) {
    return false;
  }
  out = todayContext;
  return true;
}
