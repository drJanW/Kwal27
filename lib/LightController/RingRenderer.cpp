/**
 * @file RingRenderer.cpp
 * @brief Direct ring renderer — 6 concentric PMMA circles, per-ring color + brightness
 * @version 260605D
 * @date 2026-06-05
 */
#include <Arduino.h>
#include "RingRenderer.h"
#include "MathUtils.h"

extern CRGB leds[];

// Ring start indices (from PCB layout — 6 concentric PMMA circles)
static constexpr uint8_t ringStart[RING_COUNT + 1] = { 0, 8, 24, 48, 80, 116, 160 };

// Current and target state per ring
static CRGB    ringCurrent[RING_COUNT]    = {};
static CRGB    ringTargetColor[RING_COUNT]     = {};
static uint8_t ringBriCurrent[RING_COUNT] = {};
static uint8_t ringBriTarget[RING_COUNT]  = {};

// Lerp speed: fraction per 50ms frame. 0.20 ≈ brisk ~250ms transition
static constexpr float lerpSpeed = 0.20f;

void setRingTargets(const RingTarget targets[RING_COUNT]) {
    for (int z = 0; z < RING_COUNT; z++) {
        ringTargetColor[z] = targets[z].color;
        ringBriTarget[z]   = targets[z].brightness;
        if (targets[z].instant) {
            ringCurrent[z]    = targets[z].color;
            ringBriCurrent[z] = targets[z].brightness;
        }
    }
}

static inline uint8_t lerpByte(uint8_t current, uint8_t target, float t) {
    return current + static_cast<uint8_t>((static_cast<int16_t>(target) - current) * t);
}

void renderRings() {
    // Advance current toward target
    for (int z = 0; z < RING_COUNT; z++) {
        ringCurrent[z].r = lerpByte(ringCurrent[z].r, ringTargetColor[z].r, lerpSpeed);
        ringCurrent[z].g = lerpByte(ringCurrent[z].g, ringTargetColor[z].g, lerpSpeed);
        ringCurrent[z].b = lerpByte(ringCurrent[z].b, ringTargetColor[z].b, lerpSpeed);
        ringBriCurrent[z] = lerpByte(ringBriCurrent[z], ringBriTarget[z], lerpSpeed);
    }

    // Write to LED buffer — each ring gets its color
    for (int z = 0; z < RING_COUNT; z++) {
        CRGB color = ringCurrent[z];
        uint8_t bri = ringBriCurrent[z];

        CRGB pixel = color;
        if (bri > 0) pixel.nscale8_video(bri);
        else         pixel = CRGB::Black;

        for (int i = ringStart[z]; i < ringStart[z + 1]; i++) {
            leds[i] = pixel;
        }
    }

    FastLED.show();
}
