/**
 * @file ClockPolicy.cpp
 * @brief RTC/NTP clock business logic implementation
 * @version 260212H
 * @date 2026-02-12
 */
#include <Arduino.h>
#include "ClockPolicy.h"

#include "Globals.h"
#include "I2CInitHelper.h"
#include "PRTClock.h"
#include "RTCController.h"
#include "Alert/AlertRun.h"
#include "Alert/AlertState.h"
#include "TimerManager.h"

namespace {
    bool probeRtc() {
        RTCController::begin();
        if (RTCController::isAvailable()) {
            if (RTCController::wasPowerLost()) {
                PL("[RTC] Power lost; set time manually");
            }
            return true;
        }
        return false;
    }

    void cb_rtcInit() {
        I2CInitHelper::tryInit(SC_RTC);
    }
}

namespace ClockPolicy {

void begin() {
    I2CInitHelper::start({
        "RTC", SC_RTC,
        probeRtc,
        10, 1000, 1.5f,
        AlertRequest::RTC_OK, AlertRequest::RTC_FAIL
    }, cb_rtcInit);
}

bool isRtcAvailable() {
    return I2CInitHelper::isReady(SC_RTC);
}

bool seedClockFromRTC(PRTClock &clock) {
    if (!I2CInitHelper::isReady(SC_RTC)) {
        return false;
    }
    return RTCController::readInto(clock);
}

void syncRTCFromClock(const PRTClock &clock) {
    if (!I2CInitHelper::isReady(SC_RTC)) {
        return;
    }
    RTCController::writeFrom(clock);
}

} // namespace ClockPolicy
