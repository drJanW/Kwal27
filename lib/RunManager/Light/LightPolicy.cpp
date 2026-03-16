/**
 * @file LightPolicy.cpp
 * @brief LED show business logic implementation
 * @version 260314C
 * @date 2026-03-14
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
    // Combined multiplier (calendar shift + web + SNB normalisation)
    // During calibration: skip calendarShift — user controls brightness via slider only
    float calMult = Globals::luxCalibrationMode ? 1.0f : (1.0f + (calendarShift / 100.0f));

    float brightness;
    if (!Globals::luxSensorPresent) {
        // No lux sensor: brightness driven by brightnessHi × multipliers
        brightness = Globals::brightnessHi * calMult * webMultiplier * cnf * pnf;
    } else {
        // Exponential saturation: lux → brightness
        float safeLux = MathUtils::maxVal(lux, 0.0f);
        float luxBrightness = Globals::brMax * (1.0f - expf(-Globals::luxRate * safeLux));
        brightness = luxBrightness * calMult * webMultiplier * cnf * pnf;
    }

    uint8_t clamped = static_cast<uint8_t>(clamp(brightness, Globals::brightnessLo, Globals::brightnessHi));

    // Calibration mode: store clamped brightness (= what FastLED receives)
    if (Globals::luxCalibrationMode) {
        Globals::lastFastledBrightness = static_cast<float>(clamped);
    }

    return clamped;
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
