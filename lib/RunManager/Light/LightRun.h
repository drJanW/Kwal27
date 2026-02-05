/**
 * @file LightRun.h
 * @brief LED show state management
 * @version 260202A
 * @date 2026-02-02
 */
#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <cstdint>

struct LightPattern;
struct LightColor;

// Source of current pattern/color selection
enum class LightSource : uint8_t {
    CONTEXT = 0,   // Default/boot state, no explicit selection
    MANUAL,        // User selected via web UI
    CALENDAR       // Calendar-driven selection
};

class LightRun {
public:
    void plan();

    static void updateDistance(float distanceMm);

    // Timer callbacks (cb_ prefix per rule 1.3)
    static void cb_animation();
    static void cb_shiftTimer();
    static void cb_luxMeasure();
    static void cb_luxMeasureRead();
    static void cb_tryLuxMeasure();
    static void cb_cooldownExpired();

    // Slider-triggered lux measurement (B6: debounce + 100ms cooldown)
    static void requestLuxMeasurement();

    // Web brightness modifier (0-1) - routes to LightController
    static void setWebBrightnessModifier(float modifier);

    // Light pattern/color exports routed through run
    static bool patternRead(String &payload, String &activePatternId);
    static bool colorRead(String &payload, String &activeColorId);

    // Get current source for pattern/color
    static LightSource getPatternSource();
    static LightSource getColorSource();

    // Request methods for TodayState (returns TodayModels structs)
    static bool describePatternById(uint8_t id, LightPattern& out);
    static bool describeActivePattern(LightPattern& out);
    static bool describeColorById(uint8_t id, LightColor& out);
    static bool describeActiveColor(LightColor& out);

    // Calendar-driven selection (clears manual override, applies calendar pattern/color)
    static void applyPattern(uint8_t patternId);
    static void applyColor(uint8_t colorId);

    // Apply combined pattern+color to lights (call after any pattern/color change)
    static void applyToLights();
    
    // Reapply current pattern/color (used by AlertRGB to restore after alert)
    static void reapplyCurrentShow();

    static bool selectPattern(const String &id, String &errorMessage);
    static bool selectNextPattern(String &errorMessage);
    static bool selectPrevPattern(String &errorMessage);
    static bool updatePattern(JsonVariantConst body, String &affectedId, String &errorMessage);
    static bool deletePattern(JsonVariantConst body, String &affectedId, String &errorMessage);

    static bool selectColor(const String &id, String &errorMessage);
    static bool selectNextColor(String &errorMessage);
    static bool selectPrevColor(String &errorMessage);
    static bool updateColor(JsonVariantConst body, String &affectedId, String &errorMessage);
    static bool deleteColorSet(JsonVariantConst body, String &affectedId, String &errorMessage);

    static bool previewPattern(JsonVariantConst body, String &errorMessage);
    static bool previewColor(JsonVariantConst body, String &errorMessage);
};
