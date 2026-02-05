/**
 * @file LightPolicy.cpp
 * @brief LED show business logic implementation
 * @version 260131A
 * @date 2026-01-31
 */
#include <Arduino.h>
#include <math.h>

#include "LightPolicy.h"
#include "Globals.h"

namespace LightPolicy {

float applyBrightnessRules(float requested) {
    return clamp(requested, 0.0f, static_cast<float>(Globals::maxBrightness));
}

uint8_t calcShiftedHi(float lux, int8_t calendarShift, float webShift) {
    // Combined lux + calendar + web shift → shiftedHi
    
    // luxShift from lux
    float luxShift = MathUtils::mapRange(
        clamp(lux, Globals::luxMin, Globals::luxMax),
        Globals::luxMin, Globals::luxMax,
        static_cast<float>(Globals::luxShiftLo), 
        static_cast<float>(Globals::luxShiftHi));
    
    // Combined multiplier (webShift can be >1.0 to override other shifts)
    float combinedMultiplier = 
        (1.0f + (luxShift / 100.0f)) * 
        (1.0f + (static_cast<float>(calendarShift) / 100.0f)) *
        webShift;
    
    // Map directly to brightness range, clamp to valid bounds
    float brightness = static_cast<float>(Globals::brightnessHi) * combinedMultiplier;
    return static_cast<uint8_t>(clamp(brightness, 
        static_cast<float>(Globals::brightnessLo), 
        static_cast<float>(Globals::brightnessHi)));
}

bool distanceAnimationFor(float distanceMm,
                          uint32_t& frameIntervalMs,
                          float& intensity,
                          uint8_t& paletteId) {
    (void)distanceMm;
    (void)frameIntervalMs;
    (void)intensity;
    (void)paletteId;
    // TODO: Implement distance-driven RGB lightshow modulation.
    frameIntervalMs = 0;
    intensity = 0.0f;
    paletteId = 0;
    return false;
}

}
