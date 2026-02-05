/**
 * @file ClockPolicy.cpp
 * @brief RTC/NTP clock business logic implementation
 * @version 260204A
 * @date 2026-02-04
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

#include <Wire.h>
#include <RTClib.h>
#include <sys/time.h>

namespace {
    bool probeRtc() {
        RTCController::begin();
        if (RTCController::isAvailable()) {
            if (RTCController::rtc.lostPower()) {
                PL("[RTC] Power lost; set time manually");
            }
            return true;
        }
        return false;
    }

    void cb_rtcInit() {
        I2CInitHelper::tryInit(SC_RTC);
    }

    DateTime buildDateTimeFromClock(const PRTClock &clock) {
        uint16_t year = static_cast<uint16_t>(2000U + clock.getYear());
        uint8_t month = clock.getMonth();
        uint8_t day = clock.getDay();
        uint8_t hour = clock.getHour();
        uint8_t minute = clock.getMinute();
        uint8_t second = clock.getSecond();
        return DateTime(year, month ? month : 1, day ? day : 1,
                        hour, minute, second);
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
    DateTime now = RTCController::rtc.now();
    if (now.year() < 2000 || now.year() > 2099) {
        return false;
    }
    clock.setYear(now.year() - 2000);
    clock.setMonth(now.month());
    clock.setDay(now.day());
    clock.setHour(now.hour());
    clock.setMinute(now.minute());
    clock.setSecond(now.second());
    clock.setDoW(now.year(), now.month(), now.day());
    clock.setDoY(now.year(), now.month(), now.day());
    clock.setMoonPhaseValue();

    // Align ESP32 system clock so SD timestamps are correct before Wi-Fi/NTP
    struct timeval tv;
    tv.tv_sec = static_cast<time_t>(now.unixtime());
    tv.tv_usec = 0;
    settimeofday(&tv, nullptr);
    PF("[RTC] Seeded clock from RTC read (%04d-%02d-%02d %02d:%02d:%02d)\n",
       now.year(), now.month(), now.day(),
       now.hour(), now.minute(), now.second());
    return true;
}

void syncRTCFromClock(const PRTClock &clock) {
    if (!I2CInitHelper::isReady(SC_RTC)) {
        return;
    }
    DateTime dt = buildDateTimeFromClock(clock);
    if (dt.year() < 2000 || dt.year() > 2099) {
        return;
    }
    RTCController::rtc.adjust(dt);
    PF("[RTC] Synced hardware clock to %04d-%02d-%02d %02d:%02d:%02d\n",
       dt.year(), dt.month(), dt.day(),
       dt.hour(), dt.minute(), dt.second());
}

} // namespace ClockPolicy
