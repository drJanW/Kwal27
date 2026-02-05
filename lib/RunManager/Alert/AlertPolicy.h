/**
 * @file AlertPolicy.h
 * @brief Hardware failure alert business logic
 * @version 260131A
 * @date 2026-01-31
 */
#pragma once

#include <stdint.h>
#include <FastLED.h>

namespace AlertPolicy {
    void configure();
    
    // RGB cycle timing - critical failures get longer flash
    constexpr uint32_t BLINK_CRITICAL_MS = 2000;  // SD, WiFi, Audio, RGB
    constexpr uint32_t BLINK_OTHER_MS    = 1000;  // RTC, NTP, sensors
    constexpr uint32_t BLINK_GAP_MS      = 200;   // Gap between colors
    
    // Legacy - kept for compatibility
    constexpr uint32_t BLINK_ON_MS  = 500;
    constexpr uint32_t BLINK_OFF_MS = 500;
    
    // Hardcoded failure colors per component
    constexpr uint32_t COLOR_SD      = 0xFF0000;  // Rood
    constexpr uint32_t COLOR_WIFI    = 0xFF8000;  // Oranje
    constexpr uint32_t COLOR_RTC     = 0xFFFF00;  // Geel
    constexpr uint32_t COLOR_NTP     = 0xFFFFFF;  // Wit
    constexpr uint32_t COLOR_DISTANCE_SENSOR = 0x00FF00;  // Groen
    constexpr uint32_t COLOR_LUX_SENSOR = 0xFF00FF;  // Magenta
    constexpr uint32_t COLOR_SENSOR3 = 0x00FFFF;  // Cyaan
}