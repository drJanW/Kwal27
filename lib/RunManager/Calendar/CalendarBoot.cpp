/**
 * @file CalendarBoot.cpp
 * @brief Calendar subsystem one-time initialization implementation
 * @version 260206A
 * @date 2026-02-06
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

void armRetry();
void cancelRetry();

void cb_retry() {
  // Update boot status with remaining retries
  auto remaining = timers.remaining();
  AlertState::set(SC_CALENDAR, remaining);

  // Check if timer exhausted
  if (remaining == 1) {
    AlertState::setStatusOK(SC_CALENDAR, false);
    PF("[CalendarBoot] Gave up after 14 retries\n");
    return;
  }

  calendarBoot.plan();
}

void armRetry() {
  // Growing interval: starts at 2s, grows 1.5x each retry, max 30s
  // Use restart() because armRetry() can be called while timer is pending
  if (!timers.restart(retryStartMs, retryCount, cb_retry, retryGrowth)) {
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
