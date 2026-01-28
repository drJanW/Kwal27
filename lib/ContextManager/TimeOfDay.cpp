/**
 * @file TimeOfDay.cpp
 * @brief Time-of-day period detection implementation
 * @version 251231E
 * @date 2025-12-31
 *
 * Implements time-of-day period detection based on current clock time.
 * Defines time boundaries for periods like night, dawn, morning, day,
 * afternoon, dusk, and evening. Provides functions to check current
 * time period and generate bitmasks for context-aware behavior.
 * Time boundaries are configurable via constants.
 */

#include "TimeOfDay.h"
#include "ContextStatus.h"
#include "PRTClock.h"

// Time boundaries (in minutes from midnight)
// Adjust these values to match your preferred time-of-day definitions
namespace {
    constexpr int kDawnStart    = 5 * 60;       // 05:00
    constexpr int kMorningStart = 7 * 60;       // 07:00
    constexpr int kDayStart     = 9 * 60;       // 09:00
    constexpr int kAfternoonStart = 12 * 60;    // 12:00
    constexpr int kDuskStart    = 17 * 60;      // 17:00
    constexpr int kEveningStart = 19 * 60;      // 19:00
    constexpr int kNightStart   = 22 * 60;      // 22:00
    
    // Fallback values when sun fetch hasn't succeeded yet
    constexpr int kFallbackSunrise = 7 * 60;    // 07:00
    constexpr int kFallbackSunset  = 19 * 60;   // 19:00
    
    int getCurrentMinutes() {
        return prtClock.getHour() * 60 + prtClock.getMinute();
    }
    
    int getSunriseMinutes() {
        int sunrise = prtClock.getSunriseHour() * 60 + prtClock.getSunriseMinute();
        // If no valid fetch yet (both 0), use fallback
        if (sunrise == 0 && prtClock.getSunsetHour() == 0) {
            return kFallbackSunrise;
        }
        return sunrise;
    }
    
    int getSunsetMinutes() {
        int sunset = prtClock.getSunsetHour() * 60 + prtClock.getSunsetMinute();
        // If no valid fetch yet (both 0), use fallback
        if (sunset == 0 && prtClock.getSunriseHour() == 0) {
            return kFallbackSunset;
        }
        return sunset;
    }
}

namespace TimeOfDay {

bool isNight() {
    int now = getCurrentMinutes();
    return now >= kNightStart || now < kDawnStart;
}

bool isDawn() {
    int now = getCurrentMinutes();
    int sunrise = getSunriseMinutes();
    // Dawn: from 1 hour before sunrise to sunrise
    int dawnStart = sunrise - 60;
    if (dawnStart < 0) dawnStart += 24 * 60;
    return (dawnStart <= now && now < sunrise) ||
           (dawnStart > sunrise && (now >= dawnStart || now < sunrise));
}

bool isMorning() {
    int now = getCurrentMinutes();
    return now >= kMorningStart && now < kAfternoonStart;
}

bool isLight() {
    int now = getCurrentMinutes();
    int sunrise = getSunriseMinutes();
    int sunset = getSunsetMinutes();
    return now >= sunrise && now < sunset;
}

bool isDay() {
    int now = getCurrentMinutes();
    return now >= kDayStart && now < kDuskStart;
}

bool isAfternoon() {
    int now = getCurrentMinutes();
    return now >= kAfternoonStart && now < kDuskStart;
}

bool isDusk() {
    int now = getCurrentMinutes();
    int sunset = getSunsetMinutes();
    // Dusk: from sunset to 1 hour after sunset
    int duskEnd = sunset + 60;
    if (duskEnd >= 24 * 60) duskEnd -= 24 * 60;
    return (now >= sunset && now < duskEnd) ||
           (duskEnd < sunset && (now >= sunset || now < duskEnd));
}

bool isEvening() {
    int now = getCurrentMinutes();
    return now >= kEveningStart && now < kNightStart;
}

bool isDark() {
    return !isLight();
}

bool isAM() {
    int now = getCurrentMinutes();
    return now < 12 * 60;
}

bool isPM() {
    return !isAM();
}

uint64_t getActiveStatusBits() {
    uint64_t bits = 0;
    
    if (isNight())     bits |= (1ULL << STATUS_NIGHT);
    if (isDawn())      bits |= (1ULL << STATUS_DAWN);
    if (isMorning())   bits |= (1ULL << STATUS_MORNING);
    if (isLight())     bits |= (1ULL << STATUS_LIGHT);
    if (isDay())       bits |= (1ULL << STATUS_DAY);
    if (isAfternoon()) bits |= (1ULL << STATUS_AFTERNOON);
    if (isDusk())      bits |= (1ULL << STATUS_DUSK);
    if (isEvening())   bits |= (1ULL << STATUS_EVENING);
    if (isDark())      bits |= (1ULL << STATUS_DARK);
    if (isAM())        bits |= (1ULL << STATUS_AM);
    if (isPM())        bits |= (1ULL << STATUS_PM);
    
    return bits;
}

} // namespace TimeOfDay
