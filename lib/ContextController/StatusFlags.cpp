/**
 * @file StatusFlags.cpp
 * @brief Hardware failure bits and status tracking implementation
 * @version 260204A
 $12026-02-11
 */
#include <Arduino.h>
#include "StatusFlags.h"
#include "StatusBits.h"
#include "TimeOfDay.h"
#include "ContextController.h"
#include "Alert/AlertState.h"
#include "Globals.h"
#include <math.h>
#include "HWconfig.h"

namespace StatusFlags {

// ============================================================
// Season detection (based on month, Northern Hemisphere)
// ============================================================
uint64_t getSeasonBits() {
    uint64_t bits = 0;
    const auto& ctx = ContextController::time();
    uint8_t month = ctx.month;
    
    // Spring: March (3), April (4), May (5)
    if (month >= 3 && month <= 5) {
        bits |= (1ULL << STATUS_SPRING);
    }
    // Summer: June (6), July (7), August (8)
    else if (month >= 6 && month <= 8) {
        bits |= (1ULL << STATUS_SUMMER);
    }
    // Autumn: September (9), October (10), November (11)
    else if (month >= 9 && month <= 11) {
        bits |= (1ULL << STATUS_AUTUMN);
    }
    // Winter: December (12), January (1), February (2)
    else {
        bits |= (1ULL << STATUS_WINTER);
    }
    
    return bits;
}

// ============================================================
// Weekday detection (from dayOfWeek: 0=Sunday, 1=Monday, etc.)
// ============================================================
uint64_t getWeekdayBits() {
    uint64_t bits = 0;
    const auto& ctx = ContextController::time();
    uint8_t dow = ctx.dayOfWeek;  // 0=Sun, 1=Mon, 2=Tue, ... 6=Sat
    
    // Map dayOfWeek to individual day flags
    switch (dow) {
        case 0: bits |= (1ULL << STATUS_SUNDAY);    break;
        case 1: bits |= (1ULL << STATUS_MONDAY);    break;
        case 2: bits |= (1ULL << STATUS_TUESDAY);   break;
        case 3: bits |= (1ULL << STATUS_WEDNESDAY); break;
        case 4: bits |= (1ULL << STATUS_THURSDAY);  break;
        case 5: bits |= (1ULL << STATUS_FRIDAY);    break;
        case 6: bits |= (1ULL << STATUS_SATURDAY);  break;
    }
    
    // Weekend flag (Saturday or Sunday)
    if (dow == 0 || dow == 6) {
        bits |= (1ULL << STATUS_WEEKEND);
    }
    
    return bits;
}

// ============================================================
// Weather/temperature detection (from fetched outdoor temp)
// Uses average of min/max for "current" feel
// ============================================================
uint64_t getWeatherBits() {
    uint64_t bits = 0;
    const auto& ctx = ContextController::time();
    
    if (!ctx.hasWeather) {
        // No weather data yet - return no weather flags
        return bits;
    }
    
    // Use average of daily min/max as "ambient" temperature
    float avgTemp = (ctx.weatherMinC + ctx.weatherMaxC) / 2.0f;
    
    // Classify into temperature bands
    if (avgTemp < 0.0f) {
        bits |= (1ULL << STATUS_FREEZING);
    } else if (avgTemp < 10.0f) {
        bits |= (1ULL << STATUS_COLD);
    } else if (avgTemp < 20.0f) {
        bits |= (1ULL << STATUS_MILD);
    } else if (avgTemp < 30.0f) {
        bits |= (1ULL << STATUS_WARM);
    } else {
        bits |= (1ULL << STATUS_HOT);
    }
    
    return bits;
}

// ============================================================
// RTC temperature shift (indoor temp)
// ============================================================
uint64_t getTemperatureShiftBits() {
    const auto& ctx = ContextController::time();
    if (!ctx.hasRtcTemperature) {
        return 0;
    }
    return (1ULL << STATUS_TEMPERATURE_SHIFT);
}

float getTemperatureShiftScale() {
    const auto& ctx = ContextController::time();
    if (!ctx.hasRtcTemperature) {
        return 0.0f;
    }
    float tempC = ctx.rtcTemperatureC;
    if (isnan(tempC)) {
        return 0.0f;
    }
    if (tempC > 40.0f) {
        return 0.0f;
    }
    const float normalized = clamp((tempC - 15.0f) / 15.0f, 0.0f, 1.0f);
    return (normalized - 0.5f) * 2.0f;
}

// ============================================================
// Moon phase detection (from moonPhase: 0=new, 0.5=full, 1=new)
// ============================================================
uint64_t getMoonPhaseBits() {
    uint64_t bits = 0;
    const auto& ctx = ContextController::time();
    float phase = ctx.moonPhase;  // 0.0 to 1.0
    
    // Divide lunar cycle into 4 phases:
    // New Moon:   0.00 - 0.125 or 0.875 - 1.00 (dark moon)
    // Waxing:     0.125 - 0.375 (growing toward full)
    // Full Moon:  0.375 - 0.625 (bright moon)
    // Waning:     0.625 - 0.875 (shrinking toward new)
    
    if (phase < 0.125f || phase >= 0.875f) {
        bits |= (1ULL << STATUS_NEW_MOON);
    } else if (phase < 0.375f) {
        bits |= (1ULL << STATUS_WAXING);
    } else if (phase < 0.625f) {
        bits |= (1ULL << STATUS_FULL_MOON);
    } else {
        bits |= (1ULL << STATUS_WANING);
    }
    
    return bits;
}

// ============================================================
// Time-of-day (delegates to existing TimeOfDay namespace)
// ============================================================
uint64_t getTimeOfDayBits() {
    return TimeOfDay::getActiveStatusBits();
}

// ============================================================
// Hardware status flags (bit set = NOT OK)
// ============================================================
uint64_t getHardwareFailBits() {
    uint64_t bits = 0;
    // SD and WiFi always required
    if (!AlertState::isSdOk())             bits |= (1ULL << STATUS_SD_OK);
    if (!AlertState::isWifiOk())           bits |= (1ULL << STATUS_WIFI_OK);
    // Optional hardware: only count as fail if PRESENT
    if (RTC_PRESENT && !AlertState::isRtcOk())                       bits |= (1ULL << STATUS_RTC_OK);
    if (!AlertState::isNtpOk())                                      bits |= (1ULL << STATUS_NTP_OK);
    if (DISTANCE_SENSOR_PRESENT && !AlertState::isDistanceSensorOk()) bits |= (1ULL << STATUS_DISTANCE_SENSOR_OK);
    if (LUX_SENSOR_PRESENT && !AlertState::isLuxSensorOk())           bits |= (1ULL << STATUS_LUX_SENSOR_OK);
    if (SENSOR3_PRESENT && !AlertState::isSensor3Ok())                bits |= (1ULL << STATUS_SENSOR3_OK);
    if (!AlertState::isNasOk())                                       bits |= (1ULL << STATUS_NAS_OK);
    return bits;
}

// ============================================================
// Combined: all status flags OR'd together
// ============================================================
uint64_t getFullStatusBits() {
    uint64_t bits = 0;
    
    bits |= getTimeOfDayBits();
    bits |= getSeasonBits();
    bits |= getWeekdayBits();
    bits |= getWeatherBits();
    bits |= getTemperatureShiftBits();
    bits |= getMoonPhaseBits();
    bits |= getHardwareFailBits();
    
    return bits;
}

} // namespace StatusFlags