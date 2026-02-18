/**
 * @file CalendarBoot.cpp
 * @brief Calendar subsystem one-time initialization implementation
 * @version 260218B
 * @date 2026-02-18
 */
#include <Arduino.h>
#include "CalendarBoot.h"

#include "Calendar.h"
#include "Globals.h"
#include "TimerManager.h"
#include "PRTClock.h"
#include "Alert/AlertState.h"
#include <SD.h>

bool InitTodayState(fs::FS& sd, const char* rootPath = "/");

CalendarBoot calendarBoot;

namespace {

constexpr uint32_t retryStartMs = 2UL * 1000UL;  // Start retry interval
constexpr uint8_t  retryCount   = 14;            // 14 retries with growing interval
constexpr float    retryGrowth  = 1.5f;          // Interval multiplier per retry

// Log-once flags
bool loggedSdWait = false;
bool loggedClockWait = false;
bool loggedInitFail = false;
bool loggedStateFail = false;

/// Attempt all CalendarBoot work. Returns true if fully done.
bool tryBoot() {
  if (!AlertState::isSdOk()) {
    if (!loggedSdWait) {
      PF("[CalendarBoot] SD not ready, retrying\n");
      loggedSdWait = true;
    }
    return false;
  }
  loggedSdWait = false;

  if (!prtClock.hasValidDate()) {
    if (!loggedClockWait) {
      PF_BOOT("[CalendarBoot] Waiting for clock\n");
      loggedClockWait = true;
    }
    return false;
  }
  loggedClockWait = false;

  if (!calendarSelector.isReady()) {
    if (!calendarSelector.begin(SD)) {
      if (!loggedInitFail) {
        PF("[CalendarBoot] Calendar selector init failed\n");
        loggedInitFail = true;
      }
      return false;
    }
    loggedInitFail = false;
    PF_BOOT("[CalendarBoot] selector ready\n");
  }

  if (!InitTodayState(SD)) {
    if (!loggedStateFail) {
      PF("[CalendarBoot] Today state init failed\n");
      loggedStateFail = true;
    }
    return false;
  }
  loggedStateFail = false;

  PF_BOOT("[CalendarBoot] today state ready\n");
  AlertState::setStatusOK(SC_CALENDAR);
  return true;
}

void cb_retry() {
  auto remaining = timers.remaining();
  AlertState::set(SC_CALENDAR, remaining);

  if (tryBoot()) {
    timers.cancel(cb_retry);
    return;
  }

  if (remaining <= 1) {
    AlertState::setStatusOK(SC_CALENDAR, false);
    PF("[CalendarBoot] Gave up after %u retries\n", retryCount);
  }
}

} // namespace

void CalendarBoot::plan() {
  if (tryBoot()) {
    return;
  }

  // Failed — arm repeating retry timer (counts down naturally with growing interval)
  if (!timers.create(retryStartMs, retryCount, cb_retry, retryGrowth)) {
    PF("[CalendarBoot] Failed to create retry timer\n");
  }
}
