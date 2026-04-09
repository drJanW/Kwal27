/**
 * @file CalendarRun.cpp
 * @brief Calendar state management implementation
 * @version 260409K
 * @date 2026-04-09
 */
#include <Arduino.h>
#include "CalendarRun.h"

#include "CalendarPolicy.h"
#include "Calendar.h"
#include "RunManager.h"
#include "Globals.h"
#include "TimerManager.h"
#include "PRTClock.h"
#include "SDController.h"
#include "TodayState.h"
#include "Light/LightRun.h"
#include "Alert/AlertState.h"
#include "Alert/AlertRun.h"
#include "PlaySentence.h"
#include "PlayFragment.h"
#include "AudioState.h"
#include "Audio/AudioPolicy.h"

namespace {

constexpr uint32_t retryStartMs      = 2UL * 1000UL;   // Start retry interval (grows)
constexpr uint8_t  retryCount        = 50;             // 50 retries with growing interval
constexpr float    retryGrowth       = 1.5f;           // Interval multiplier per retry
constexpr uint32_t initialDelayMs    = 5UL * 1000UL;

bool clockReady() {
  return prtClock.hasValidDate();
}

struct CalendarRunLogFlags {
  bool controllerNotReady = false;
  bool sdBusy = false;
};

CalendarRunLogFlags logFlags;
bool initialDelayPending = true;

TodayState todayState;
bool todayStateValid = false;

void resetLogFlags() {
  logFlags.controllerNotReady = false;
  logFlags.sdBusy = false;
}

void clearTodayStateRead() {
  todayState = TodayState{};
  todayStateValid = false;
}

void refreshTodayStateRead() {
  TodayState state;
  if (LoadTodayState(state) && state.valid) {
    todayState = state;
    todayStateValid = true;
  } else {
    clearTodayStateRead();
  }
}

String sentence;
uint32_t sentenceIntervalMs = 0;
uint32_t calSentenceDurationMs = 0;

bool playCalendarSentenceFromCache() {
  if (calSentenceDurationMs == 0) return false;
  AudioFragment frag{};
  frag.dirIndex   = Globals::ttsCacheDirIndex;
  frag.fileIndex  = Globals::ttsCacheCalFileIndex;
  frag.durationMs = calSentenceDurationMs;
  frag.fadeMs     = 500;
  strlcpy(frag.source, "calendar", sizeof(frag.source));
  return AudioPolicy::requestFragment(frag);
}

bool downloadCalendarSentence(const String& text) {
  if (text.isEmpty()) return false;
  if (isFragmentPlaying()) PlayAudioFragment::stop(0);
  if (isSentencePlaying()) PlaySentence::stop();
  uint32_t durationMs = PlaySentence::downloadTtsToCache(
      text.c_str(), -1, 99, Globals::ttsCacheCalFileIndex);
  if (durationMs == 0) {
    PF("[Calendar] TTS cache download failed\n");
    return false;
  }
  calSentenceDurationMs = durationMs;
  return true;
}

void clearSentenceTimer() {
  timers.cancel(CalendarRun::cb_downloadCalendarSentence);
  timers.cancel(CalendarRun::cb_calendarSentence);
  sentence = "";
  sentenceIntervalMs = 0;
  calSentenceDurationMs = 0;
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

  if (!calendarSelector.isReady()) {
    if (!logFlags.controllerNotReady) {
      PF_BOOT("[CalendarRun] waiting for CSV\n");
      logFlags.controllerNotReady = true;
    }
    timers.restart(retryStartMs, retryCount, CalendarRun::cb_loadCalendar, retryGrowth);
    return;
  }
  logFlags.controllerNotReady = false;

  if (!clockReady()) {
    timers.restart(retryStartMs, retryCount, CalendarRun::cb_loadCalendar, retryGrowth);
    return;
  }

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
  if (!calendarSelector.isReady()) {
    if (!logFlags.controllerNotReady) {
      PF_BOOT("[CalendarRun] waiting for controller\n");
      logFlags.controllerNotReady = true;
    }
    timers.restart(retryStartMs, retryCount, CalendarRun::cb_loadCalendar, retryGrowth);
    return;
  }
  logFlags.controllerNotReady = false;

  if (!clockReady()) {
    timers.restart(retryStartMs, retryCount, CalendarRun::cb_loadCalendar, retryGrowth);
    return;
  }

  uint16_t year = 0;
  uint8_t month = 0;
  uint8_t day = 0;
  if (!getValidDate(year, month, day)) {
    timers.restart(retryStartMs, retryCount, CalendarRun::cb_loadCalendar, retryGrowth);
    return;
  }

  bool calendarLoaded = false;
  {
    if (AlertState::isSdBusy()) {
      if (!logFlags.sdBusy) {
        PF("[CalendarRun] SD busy, retrying\n");
        logFlags.sdBusy = true;
      }
      timers.restart(retryStartMs, retryCount, CalendarRun::cb_loadCalendar, retryGrowth);
      return;
    }
    logFlags.sdBusy = false;
    SDController::lockSD();
    calendarLoaded = calendarSelector.loadToday(year, month, day);
    SDController::unlockSD();
  }

  if (!calendarLoaded) {
    clearSentenceTimer();
    CalendarPolicy::applyThemeBox(CalendarThemeBox{});
    LightRun::applyPattern(0);
    LightRun::applyColor(0);
    clearTodayStateRead();
    AlertState::setCalendarStatus(true);  // OK - no special day
    AlertRun::playWelcomeIfPending();
    RunManager::triggerBootFragment();  // Theme box set, play first fragment
    PF("[Calendar] No entry today\n");
    timers.restart(Globals::calendarRefreshIntervalMs, 0, CalendarRun::cb_loadCalendar);
    return;
  }

  const auto& calData = calendarSelector.calendarData();
  CalendarPolicy::Decision decision;
  if (!CalendarPolicy::evaluate(calData, decision)) {
    clearSentenceTimer();
    CalendarPolicy::applyThemeBox(CalendarThemeBox{});
    LightRun::applyPattern(0);
    LightRun::applyColor(0);
    clearTodayStateRead();
    AlertRun::playWelcomeIfPending();
    timers.restart(Globals::calendarRefreshIntervalMs, 0, CalendarRun::cb_loadCalendar);
    return;
  }

  if (decision.hasSentence) {
    sentence = calData.day.ttsSentence;
    sentenceIntervalMs = decision.sentenceIntervalMs;

    // Defer HTTP download to separate callback (max 5s block)
    timers.create(1, 1, CalendarRun::cb_downloadCalendarSentence);
  } else {
    clearSentenceTimer();
  }

  if (decision.hasThemeBox) {
    CalendarPolicy::applyThemeBox(calData.theme);
  } else {
    CalendarPolicy::applyThemeBox(CalendarThemeBox{});
  }

  // Apply calendar-driven pattern/color via LightRun requests
  LightRun::applyPattern(calData.day.patternId);
  LightRun::applyColor(calData.day.colorId);

  refreshTodayStateRead();
  AlertState::setCalendarStatus(true);
  AlertRun::playWelcomeIfPending();
  RunManager::triggerBootFragment();  // Theme box set, play first fragment
  PL("[CalendarRun] Calendar loaded");
  timers.restart(Globals::calendarRefreshIntervalMs, 0, CalendarRun::cb_loadCalendar);
  resetLogFlags();
}

void CalendarRun::cb_downloadCalendarSentence() {
  if (sentence.isEmpty()) return;

  bool cached = downloadCalendarSentence(sentence);

  // Start repeat timer (plays from cache or live fallback)
  if (sentenceIntervalMs > 0) {
    if (!timers.restart(sentenceIntervalMs, 0, CalendarRun::cb_calendarSentence)) {
      PF("[CalendarRun] Failed to start sentence timer (%lu ms)\n",
         static_cast<unsigned long>(sentenceIntervalMs));
    }
  }

  // First play
  if (cached) {
    playCalendarSentenceFromCache();
  } else {
    CalendarPolicy::speakSentence(sentence);
  }
}

void CalendarRun::cb_calendarSentence() {
  if (sentence.isEmpty()) {
    return;
  }
  // Play from SD cache if available, otherwise fall back to live TTS
  if (calSentenceDurationMs > 0) {
    playCalendarSentenceFromCache();
  } else {
    CalendarPolicy::speakSentence(sentence);
  }
}

bool CalendarRun::todayReady() const {
  return todayStateValid && todayState.valid;
}

bool CalendarRun::todayRead(TodayState& out) const {
  if (!todayReady()) {
    return false;
  }
  out = todayState;
  return true;
}
