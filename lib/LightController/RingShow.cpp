/**
 * @file RingShow.cpp
 * @brief Unified 6-ring renderer — gradient scrolling + lerp targets
 * @version 260615B
 * @date 2026-06-15
 *
 * Merges old RingShow (4 pattern renderers) + RingRenderer (lerp target state).
 * Single ringStart[] layout, single render entry point with two color inputs:
 *   - lerp targets via setRingTargets() (TV simulator, demo RingScene)
 *   - gradient scrolling via updateRingShow() (pattern_type patterns)
 */
#include "RingShow.h"
#include <Arduino.h>

extern CRGB leds[];

// PMMA ring boundaries — identical to old TvShow/RingRenderer layout
static constexpr uint8_t ringStart[RING_COUNT + 1] = { 0, 8, 24, 48, 80, 116, 160 };

// ─── Lerp state (from old RingRenderer) ──────────────────────────────────────

static CRGB    ringCurrent[RING_COUNT]       = {};
static CRGB    ringTargetColor[RING_COUNT]   = {};
static uint8_t ringBriCurrent[RING_COUNT]    = {};
static uint8_t ringBriTarget[RING_COUNT]     = {};
static constexpr float lerpSpeed = 0.20f;     // ~250ms transition at 50ms frame

static inline uint8_t lerpByte(uint8_t current, uint8_t target, float t) {
    return current + static_cast<uint8_t>((static_cast<int16_t>(target) - current) * t);
}

// ─── Public API: lerp targets ────────────────────────────────────────────────

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

void renderRings() {
    // Advance current toward target
    for (int z = 0; z < RING_COUNT; z++) {
        ringCurrent[z].r = lerpByte(ringCurrent[z].r, ringTargetColor[z].r, lerpSpeed);
        ringCurrent[z].g = lerpByte(ringCurrent[z].g, ringTargetColor[z].g, lerpSpeed);
        ringCurrent[z].b = lerpByte(ringCurrent[z].b, ringTargetColor[z].b, lerpSpeed);
        ringBriCurrent[z] = lerpByte(ringBriCurrent[z], ringBriTarget[z], lerpSpeed);
    }

    // Write to LED buffer
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

// ─── Gradient-scrolling renderer (Rainbow/Blended) ───────────────────────────

static constexpr uint8_t ringOffset[6] = { 0, 42, 85, 128, 170, 213 };

static void renderGradientRings(const CRGB* gradient, uint8_t scrollPhase, uint8_t maxBri) {
    for (uint8_t z = 0; z < 6; z++) {
        uint8_t idx = static_cast<uint8_t>(scrollPhase + ringOffset[z]);
        CRGB color = gradient[idx];
        if (maxBri == 0) {
            color = CRGB::Black;
        } else if (maxBri < 255) {
            color.nscale8_video(maxBri);
        }
        for (int i = ringStart[z]; i < ringStart[z + 1]; i++) {
            leds[i] = color;
        }
    }
}

// ─── Static ring-color renderer ──────────────────────────────────────────────

static void renderStaticRings(const String& ringColors, uint8_t maxBri) {
    if (ringColors.isEmpty()) return;

    CRGB colors[6];
    int parsed = 0;
    int start = 0;
    for (int ring = 0; ring < 6; ring++) {
        int semi = ringColors.indexOf(';', start);
        String token = (semi >= 0)
            ? ringColors.substring(start, semi)
            : ringColors.substring(start);
        token.trim();

        int comma1 = token.indexOf(',');
        int comma2 = (comma1 >= 0) ? token.indexOf(',', comma1 + 1) : -1;
        if (comma1 >= 0 && comma2 >= 0) {
            colors[ring].r = static_cast<uint8_t>(token.substring(0, comma1).toInt());
            colors[ring].g = static_cast<uint8_t>(token.substring(comma1 + 1, comma2).toInt());
            colors[ring].b = static_cast<uint8_t>(token.substring(comma2 + 1).toInt());
            parsed++;
        } else {
            colors[ring] = CRGB::Black;
        }

        if (semi < 0) break;
        start = semi + 1;
    }

    if (parsed == 0) return;

    for (uint8_t z = 0; z < 6 && z < (uint8_t)parsed; z++) {
        CRGB color = colors[z];
        if (maxBri == 0) {
            color = CRGB::Black;
        } else if (maxBri < 255) {
            color.nscale8_video(maxBri);
        }
        for (int i = ringStart[z]; i < ringStart[z + 1]; i++) {
            leds[i] = color;
        }
    }
}

// ─── Dispatch ────────────────────────────────────────────────────────────────

void updateRingShow(const LightShowParams& params,
                    const CRGB* gradient,
                    uint8_t colorPhase,
                    uint8_t maxBri) {
    const String& type = params.patternType;

    if (type.equalsIgnoreCase("Static") && !params.ringColors.isEmpty()) {
        renderStaticRings(params.ringColors, maxBri);
    } else {
        // Rainbow, Blended, empty-string, or any unknown type → gradient scrolling
        renderGradientRings(gradient, colorPhase, maxBri);
    }
}