/**
 * @file LightPolicy.cpp
 * @brief LED show business logic implementation
 * @version 260313C
 * @date 2026-03-13
 */
#include <Arduino.h>
#include <math.h>

#include "LightPolicy.h"
#include "Globals.h"
#include "PatternCatalog.h"

namespace LightPolicy {

float applyBrightnessRules(float requested) {
    return clamp(requested, 0.0f, Globals::maxBrightness);
}

uint8_t calcShiftedHi(float lux, int8_t calendarShift, float webMultiplier,
                      float cnf, float pnf) {
    // Exponential saturation: lux → brightness
    float safeLux = MathUtils::maxVal(lux, 0.0f);
    float luxBrightness = Globals::luxBrMax * (1.0f - expf(-Globals::luxRate * safeLux));
    
    // Combined multiplier (calendar shift + web + SNB normalisation)
    float brightness = luxBrightness *
        (1.0f + (calendarShift / 100.0f)) *
        webMultiplier * cnf * pnf;

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

bool areAllPnfsCalibrated() {
    return PatternCatalog::instance().countUncalibrated() == 0;
}

}
