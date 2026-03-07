/**
 * @file TvShow.cpp
 * @brief TV simulator renderer — 6 ring zones matching PMMA circles
 * @version 260307C
 * @date 2026-03-07
 */
#include <Arduino.h>
#include "TvShow.h"
#include "MathUtils.h"

extern CRGB leds[];

// Ring start indices (from PCB layout — 6 concentric PMMA circles)
static constexpr uint8_t ringStart[TV_ZONES + 1] = { 0, 8, 24, 48, 80, 116, 160 };

// Current and target state per zone
static CRGB    zoneCurrent[TV_ZONES]    = {};
static CRGB    zoneTarget[TV_ZONES]     = {};
static uint8_t zoneBriCurrent[TV_ZONES] = {};
static uint8_t zoneBriTarget[TV_ZONES]  = {};

// Lerp speed: fraction per 50ms frame. 0.20 ≈ brisk ~250ms transition
static constexpr float kLerpSpeed = 0.20f;

void setTvZoneTargets(const TvZoneTarget targets[TV_ZONES]) {
    for (int z = 0; z < TV_ZONES; z++) {
        zoneTarget[z]    = targets[z].color;
        zoneBriTarget[z] = targets[z].brightness;
        if (targets[z].instant) {
            zoneCurrent[z]    = targets[z].color;
            zoneBriCurrent[z] = targets[z].brightness;
        }
    }
}

static inline uint8_t lerpByte(uint8_t current, uint8_t target, float t) {
    return current + static_cast<uint8_t>((static_cast<int16_t>(target) - current) * t);
}

void updateTvShow() {
    // Advance current toward target
    for (int z = 0; z < TV_ZONES; z++) {
        zoneCurrent[z].r = lerpByte(zoneCurrent[z].r, zoneTarget[z].r, kLerpSpeed);
        zoneCurrent[z].g = lerpByte(zoneCurrent[z].g, zoneTarget[z].g, kLerpSpeed);
        zoneCurrent[z].b = lerpByte(zoneCurrent[z].b, zoneTarget[z].b, kLerpSpeed);
        zoneBriCurrent[z] = lerpByte(zoneBriCurrent[z], zoneBriTarget[z], kLerpSpeed);
    }

    // Write to LED buffer — each ring gets its zone color
    for (int z = 0; z < TV_ZONES; z++) {
        CRGB color = zoneCurrent[z];
        uint8_t bri = zoneBriCurrent[z];

        CRGB pixel = color;
        if (bri > 0) pixel.nscale8_video(bri);
        else         pixel = CRGB::Black;

        for (int i = ringStart[z]; i < ringStart[z + 1]; i++) {
            leds[i] = pixel;
        }
    }

    FastLED.show();
}
