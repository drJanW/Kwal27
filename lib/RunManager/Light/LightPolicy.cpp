/**
 * @file LightPolicy.cpp
 * @brief LED show business logic implementation
 * @version 260303A
 * @date 2026-03-03
 */
#include <Arduino.h>
#include <math.h>

#include "LightPolicy.h"
#include "Globals.h"

namespace LightPolicy {

float applyBrightnessRules(float requested) {
    return clamp(requested, 0.0f, Globals::maxBrightness);
}

uint8_t calcShiftedHi(float lux, int8_t calendarShift, float webMultiplier,
                      float cnf, float pnf) {
    // Combined lux + calendar + web multiplier + SNB → shiftedHi
    
    // luxShift from lux using Stevens' power law
    // Low lux → large shift change, high lux → compressed (matches human perception)
    float normalizedLux = clamp(lux, Globals::luxMin, Globals::luxMax) / Globals::luxMax;
    float luxT = powf(normalizedLux, Globals::luxGamma);
    float luxShift = Globals::luxShiftLo +
        (Globals::luxShiftHi - Globals::luxShiftLo) * luxT;
    
    // Combined multiplier (webMultiplier can be >1.0 to override other shifts)
    float combinedMultiplier = 
        (1.0f + (luxShift / 100.0f)) * 
        (1.0f + (calendarShift / 100.0f)) *
        webMultiplier;
    
    // Apply SNB normalisation: cnf × pnf scales perceived brightness
    float brightness = Globals::brightnessHi * combinedMultiplier * cnf * pnf;

    // Calibration mode: store pre-clamp value for lux calibration samples
    if (Globals::luxCalibrationMode) {
        Globals::lastUnclampedBrightness = brightness;
    }

    return static_cast<uint8_t>(clamp(brightness, Globals::brightnessLo, Globals::brightnessHi));
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
