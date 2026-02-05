/**
 * @file LightPolicy.h
 * @brief LED show business logic
 * @version 260131A
 * @date 2026-01-31
 */
#pragma once
#include <Arduino.h>

namespace LightPolicy {

    // Apply brightness rules (caps)
    float applyBrightnessRules(float requested);

    // Calculate shiftedHi from ambient lux, calendar shift, and web shift
    // webShift can be >1.0 to override other shifts
    // Returns uint8_t Hi value (fully shifted, ready for slider mapping)
    uint8_t calcShiftedHi(float lux, int8_t calendarShift, float webShift);

    // Placeholder: distance-driven light show adjustment
    bool distanceAnimationFor(float distanceMm,
                              uint32_t& frameIntervalMs,
                              float& intensity,
                              uint8_t& paletteId);

}
