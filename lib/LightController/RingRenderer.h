/**
 * @file RingRenderer.h
 * @brief Direct ring renderer — 6 concentric PMMA circles, per-ring color + brightness
 * @version 260605D
 * @date 2026-06-05
 */
#pragma once

#include <FastLED.h>
#include "Globals.h"

#define RING_COUNT 6

struct RingTarget {
    CRGB    color;
    uint8_t brightness;
    bool    instant;    // true = hard cut (no lerp), false = smooth fade
};

// Ring boundaries: 6 concentric PMMA circles
// Ring 0: LEDs 0-7   (8 LEDs,  innermost)
// Ring 1: LEDs 8-23  (16 LEDs)
// Ring 2: LEDs 24-47 (24 LEDs)
// Ring 3: LEDs 48-79 (32 LEDs)
// Ring 4: LEDs 80-115 (36 LEDs)
// Ring 5: LEDs 116-159 (44 LEDs, outermost)

// Set new target color/brightness per ring
void setRingTargets(const RingTarget targets[RING_COUNT]);

// Render one frame: lerp current → target, write to leds[], call FastLED.show()
void renderRings();
