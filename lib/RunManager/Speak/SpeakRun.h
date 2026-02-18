/**
 * @file SpeakRun.h
 * @brief TTS speech state management
 * @version 260218M
 * @date 2026-02-18
 */
#pragma once

#include <Arduino.h>
#include <stdint.h>
#include "Alert/AlertState.h"  // for StatusComponent

/// Speech request types
enum class SpeakRequest : uint8_t {
    // Component failures (for boot notification)
    SD_FAIL,
    SD_VERSION_FAIL,
    WIFI_FAIL,
    RTC_FAIL,
    NTP_FAIL,
    DISTANCE_SENSOR_FAIL,
    LUX_SENSOR_FAIL,
    SENSOR3_FAIL,
    WEATHER_FAIL,
    CALENDAR_FAIL,
    
    // Runtime events
    DISTANCE_CLEARED,  // Object moved away from sensor
    
    // Special: say time (uses sentence)
    SAY_TIME,
    
    // Welcome greeting (time-based)
    WELCOME
};

/**
 * @class SpeakRun
 * @brief Orchestrates TTS and MP3-based speech output
 */
class SpeakRun {
public:
    /// Register speech timers with TimerManager
    void plan();
    
    /// Speak a request (TTS primary, MP3 fallback)
    static void speak(SpeakRequest request);
    
    /// Speak FAIL for component (lookup table SC_* → SpeakRequest::*_FAIL)
    static void speakFail(StatusComponent c);
    
    /// Say current time as sentence: "het is X uur Y"
    static void sayTime(uint8_t hour, uint8_t minute);
};
