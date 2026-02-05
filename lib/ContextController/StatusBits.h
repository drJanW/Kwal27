#pragma once

/**
 * @file StatusBits.h
 * @brief StatusBits implementation
 * @version 260205A
 * @date 2026-02-05
 */
#include <stdint.h>

// Time-of-day and status flags (bit positions for uint64_t bitmask)
// All flags can be combined - shifts multiply together when multiple flags active
enum TimeStatus : uint8_t {
    // Time-of-day flags (0-10)
    STATUS_NIGHT = 0,
    STATUS_DAWN,
    STATUS_MORNING,
    STATUS_LIGHT,
    STATUS_DAY,
    STATUS_AFTERNOON,
    STATUS_DUSK,
    STATUS_EVENING,
    STATUS_DARK,
    STATUS_AM,
    STATUS_PM,
    
    // Season flags (11-14) - based on month
    STATUS_SPRING,      // Mar-May
    STATUS_SUMMER,      // Jun-Aug
    STATUS_AUTUMN,      // Sep-Nov
    STATUS_WINTER,      // Dec-Feb
    
    // Weather/temperature flags (15-18) - based on outdoor temp
    STATUS_FREEZING,    // < 0°C
    STATUS_COLD,        // 0-10°C
    STATUS_MILD,        // 10-20°C
    STATUS_WARM,        // 20-30°C
    STATUS_HOT,         // > 30°C
    
    // Weekday flags (20-27)
    STATUS_MONDAY,
    STATUS_TUESDAY,
    STATUS_WEDNESDAY,
    STATUS_THURSDAY,
    STATUS_FRIDAY,
    STATUS_SATURDAY,
    STATUS_SUNDAY,
    STATUS_WEEKEND,     // Sat or Sun
    
    // Moon phase flags (28-31) - based on lunar cycle
    STATUS_NEW_MOON,    // 0-12.5% illumination
    STATUS_WAXING,      // 12.5-50% (growing)
    STATUS_FULL_MOON,   // 87.5-100% illumination
    STATUS_WANING,      // 50-87.5% (shrinking)
    
    // Hardware status flags (32-38) - bit set = NOT OK
    STATUS_SD_OK,
    STATUS_WIFI_OK,
    STATUS_RTC_OK,
    STATUS_NTP_OK,
    STATUS_DISTANCE_SENSOR_OK,
    STATUS_LUX_SENSOR_OK,
    STATUS_SENSOR3_OK,

    // Temperature shift flag (RTC temperature-based)
    STATUS_TEMPERATURE_SHIFT,
    
    STATUS_COUNT        // Must be < 64 for uint64_t bitmask
};

// Color parameters for shift system
enum ColorParam : uint8_t {
    COLOR_A_HUE = 0,
    COLOR_A_SAT,
    COLOR_B_HUE,
    COLOR_B_SAT,
    GLOBAL_BRIGHTNESS,
    COLOR_PARAM_COUNT
};

// Pattern parameters for shift system
enum PatternParam : uint8_t {
    PAT_COLOR_CYCLE = 0,
    PAT_BRIGHT_CYCLE,
    PAT_FADE_WIDTH,
    PAT_MIN_BRIGHT,
    PAT_GRADIENT_SPEED,
    PAT_CENTER_X,
    PAT_CENTER_Y,
    PAT_RADIUS,
    PAT_WINDOW_WIDTH,
    PAT_RADIUS_OSC,
    PAT_X_AMP,
    PAT_Y_AMP,
    PAT_X_CYCLE,
    PAT_Y_CYCLE,
    PAT_PARAM_COUNT
};