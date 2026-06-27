/**
 * @file RingShow.h
 * @brief Unified 6-ring renderer — gradient scrolling + lerp targets
 * @version 260615E
 * @date 2026-06-15
 *
 * Merges old RingShow (pattern dispatch) + RingRenderer (lerp targets).
 * Single ringStart[] layout, single renderRings() entry point.
 * Two color inputs:
 *   - lerp targets via setRingTargets() (TV simulator, demo RingScene)
 *   - gradient scrolling via updateRingShow() (pattern_type patterns)
 */
#pragma once

#include <FastLED.h>
#include "Globals.h"
#include "LightController.h"

#define RING_COUNT 6

struct RingTarget {
    CRGB    color;
    uint8_t brightness;
    bool    instant;    // true = hard cut (no lerp), false = smooth fade
};

/// Set new target color/brightness per ring (TV simulator, demo RingScene)
void setRingTargets(const RingTarget targets[RING_COUNT]);

/// Render one frame: lerp current → target, write to leds[], call FastLED.show()
void renderRings();

/// Ring show pattern entry point: fill leds[] for pattern_type patterns.
/// @param params      Current LightShowParams (CSV columns available)
/// @param gradient    256-entry color gradient (RGB1→RGB2) — pre-filled by caller
/// @param colorPhase  Scroll phase 0..255 (advances via cb_colorCycle)
/// @param maxBri      Brightness scale (0..255)
void updateRingShow(const LightShowParams& params,
                    const CRGB* gradient,
                    uint8_t colorPhase,
                    uint8_t maxBri);