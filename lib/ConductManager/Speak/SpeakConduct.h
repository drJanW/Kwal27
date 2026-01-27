/**
 * @file SpeakConduct.h
 * @brief TTS speech state management
 * @version 251231E
 * @date 2025-12-31
 *
 * Manages TTS speech state and sentence playback. Provides speak interface
 * for various intents (failures, time announcements), coordinates with
 * PlaySentence for word-based audio output.
 */

#pragma once

#include <Arduino.h>
#include <stdint.h>
#include "Notify/NotifyState.h"  // voor StatusComponent

// Speak intents
enum class SpeakIntent : uint8_t {
    // Component failures (for boot notification)
    SD_FAIL,
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

class SpeakConduct {
public:
    void plan();
    static void speak(SpeakIntent intent);
    
    // Speak FAIL voor component (lookup table SC_* → SpeakIntent::*_FAIL)
    static void speakFail(StatusComponent c);
    
    // Zeg de huidige tijd als zin: "het is X uur Y"
    static void sayTime(uint8_t hour, uint8_t minute);
};
