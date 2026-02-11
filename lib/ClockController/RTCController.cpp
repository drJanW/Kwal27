/**
 * @file RTCController.cpp
 * @brief Hardware RTC (DS3231) control implementation
 * @version 260204A
 $12026-02-10
 */
#include <Arduino.h>
#include "RTCController.h"
#include "Globals.h"
#include "PRTClock.h"
#include <math.h>

namespace {
    bool rtcAvailable = false;
}

namespace RTCController {

void begin() {
    rtcAvailable = rtc.begin();
    if (rtcAvailable) {
        hwStatus |= HW_RTC;
    }
}

bool isAvailable() {
    return rtcAvailable;
}

bool readInto(PRTClock& clock) {
    if (!rtcAvailable) {
        return false;
    }

    DateTime now = rtc.now();
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
    return true;
}

void writeFrom(const PRTClock& clock) {
    if (!rtcAvailable) {
        return;
    }

    uint16_t year = static_cast<uint16_t>(2000U + clock.getYear());
    uint8_t month = clock.getMonth();
    uint8_t day = clock.getDay();
    uint8_t hour = clock.getHour();
    uint8_t minute = clock.getMinute();
    uint8_t second = clock.getSecond();

    DateTime dt(year, month ? month : 1, day ? day : 1,
                hour, minute, second);
    if (dt.year() < 2000 || dt.year() > 2099) {
        return;
    }
    rtc.adjust(dt);
}

float getTemperature() {
    if (!rtcAvailable) {
        return NAN;
    }
    return rtc.getTemperature();
}

bool wasPowerLost() {
    if (!rtcAvailable) {
        return false;
    }
    return rtc.lostPower();
}

} // namespace RTCController
