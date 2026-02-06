/**
 * @file CalendarBoot.cpp
 * @brief Calendar subsystem one-time initialization implementation
 * @version 260202A
 * @date 2026-02-02
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
constexpr int32_t  retryCount   = -14;           // 14 retries with growing interval

// Log-once flags
bool loggedSdWait = false;
bool loggedClockWait = false;
bool loggedInitFail = false;
bool loggedStateFail = false;

void armRetry();
void cancelRetry();

void cb_retry() {
  // Update boot status with remaining retries
  int remaining = timers.getRepeatCount(cb_retry);
  if (remaining != -1)
    AlertState::set(SC_CALENDAR, abs(remaining));

  // Check if timer exhausted
  if (!timers.isActive(cb_retry)) {
    AlertState::setStatusOK(SC_CALENDAR, false);
    PF("[CalendarBoot] Gave up after 14 retries\n");
    return;
  }

  calendarBoot.plan();
}

void armRetry() {
  // Growing interval: starts at 2s, grows 1.5x each retry, max 30s
  // Use restart() because armRetry() can be called while timer is pending
  if (!timers.restart(retryStartMs, retryCount, cb_retry)) {
    PF("[CalendarBoot] Failed to create retry timer\n");
  }
}

void cancelRetry() {
  timers.cancel(cb_retry);
}

} // namespace

void CalendarBoot::plan() {
  if (!AlertState::isSdOk()) {
    if (!loggedSdWait) {
      PF("[CalendarBoot] SD not ready, retrying\n");
      loggedSdWait = true;
    }
    armRetry();
    return;
  }
  loggedSdWait = false;

  if (!prtClock.hasValidDate()) {
    if (!loggedClockWait) {
      PF_BOOT("[CalendarBoot] Waiting for clock\n");
      loggedClockWait = true;
    }
    armRetry();
    return;
  }
  loggedClockWait = false;

  if (!calendarSelector.isReady()) {
    if (!calendarSelector.begin(SD)) {
      if (!loggedInitFail) {
        PF("[CalendarBoot] Calendar selector init failed\n");
        loggedInitFail = true;
      }
      return;
    }
    loggedInitFail = false;
    PF_BOOT("[CalendarBoot] selector ready\n");
  }

  if (!InitTodayState(SD)) {
    if (!loggedStateFail) {
      PF("[CalendarBoot] Today state init failed\n");
      loggedStateFail = true;
    }
    armRetry();
    return;
  }
  loggedStateFail = false;

  PF_BOOT("[CalendarBoot] today state ready\n");
  AlertState::setStatusOK(SC_CALENDAR);
  cancelRetry();
}
