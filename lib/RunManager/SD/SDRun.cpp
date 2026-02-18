/**
 * @file SDRun.cpp
 * @brief SD card state management with periodic health check
 * @version 260218C
 * @date 2026-02-18
 */
#include <Arduino.h>
#include "SDRun.h"

#include "Globals.h"
#include "SDController.h"
#include "TimerManager.h"
#include "Alert/AlertState.h"
#include "Alert/AlertRun.h"

void SDRun::plan() {
    if (!AlertState::isSdOk()) {
        return;  // SD not mounted — nothing to monitor
    }
    // Start periodic health check (infinite timer, fires every sdHealthCheckIntervalMs)
    timers.create(Globals::sdHealthCheckIntervalMs, 0, cb_checkSdHealth);
}

void SDRun::cb_checkSdHealth() {
    if (!SDController::checkPresent()) {
        PF("[SDRun] SD card removed!\n");
        SDController::setReady(false);
        AlertRun::report(AlertRequest::SD_FAIL);
        timers.cancel(cb_checkSdHealth);
        return;
    }
}