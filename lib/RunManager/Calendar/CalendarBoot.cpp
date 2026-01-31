/**
 * @file CalendarBoot.cpp
 * @brief Calendar subsystem one-time initialization implementation
 * @version 251231E
 * @date 2025-12-31
 *
 * Implements calendar boot sequence with retry logic. Waits for both SD card
 * and system clock to be available before initializing TodayContext from
 * calendar CSV data on SD card.
 */

#include <Arduino.h>
#include "CalendarBoot.h"

#include "Calendar.h"
#include "Globals.h"
#include "TimerManager.h"
#include "PRTClock.h"
#include "Notify/NotifyState.h"
#include <SD.h>

bool InitTodayContext(fs::FS& sd, const char* rootPath = "/");

CalendarBoot calendarBoot;

namespace {

constexpr uint32_t retryStartMs = 2UL * 1000UL;  // Start retry interval
constexpr int32_t  retryCount   = -14;           // 14 retries with growing interval

// Log-once flags
bool loggedSdWait = false;
bool loggedClockWait = false;
bool loggedInitFail = false;
bool loggedContextFail = false;

void armRetry();
void cancelRetry();

void cb_retry() {
  // Update boot status with remaining retries
  int remaining = timers.getRepeatCount(cb_retry);
  if (remaining != -1)
    NotifyState::set(SC_CALENDAR, abs(remaining));

  // Check if timer exhausted
  if (!timers.isActive(cb_retry)) {
    NotifyState::setStatusOK(SC_CALENDAR, false);
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
  if (!NotifyState::isSdOk()) {
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
      PF("[CalendarBoot] Waiting for valid clock\n");
      loggedClockWait = true;
    }
    armRetry();
    return;
  }
  loggedClockWait = false;

  if (!calendarManager.isReady()) {
    if (!calendarManager.begin(SD)) {
      if (!loggedInitFail) {
        PF("[CalendarBoot] Calendar manager init failed\n");
        loggedInitFail = true;
      }
      return;
    }
    loggedInitFail = false;
    PF("[CalendarBoot] Calendar manager initialised\n");
  }

  if (!InitTodayContext(SD)) {
    if (!loggedContextFail) {
      PF("[CalendarBoot] Today context init failed\n");
      loggedContextFail = true;
    }
    armRetry();
    return;
  }
  loggedContextFail = false;

  PF("[CalendarBoot] Today context initialised\n");
  NotifyState::setStatusOK(SC_CALENDAR);
  cancelRetry();
}
