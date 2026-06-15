/**
 * @file RingShow.cpp
 * @brief Ring renderer dispatch + renderers for pattern #32
 * @version 260614B
 * @date 2026-06-14
 */
#include "RingShow.h"
#include <Arduino.h>

extern CRGB leds[];

// PMMA ring boundaries — identical to TvShow/RingRenderer layout
static constexpr uint8_t ringStart[7] = { 0, 8, 24, 48, 80, 116, 160 };

// ─── Renderer implementations ───────────────────────────────────────────────

// Spectrum: 6 evenly-spaced hues from a full HSV rainbow, scrolling.
static void renderSpectrum(const CRGB* gradient,
                           uint8_t scrollPhase, uint8_t maxBri) {
    static constexpr uint8_t ringOffset[6] = { 0, 42, 85, 128, 170, 213 };
    // gradient is pre-filled with 256 HSV entries by updateLightController
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

// Rainbow: 6 rings cycling through full HSV spread with phase shift per ring.
// Uses the RGB1→RGB2 gradient as the base, scrolling with ring offsets.
static void renderRainbow(const CRGB* gradient,
                          uint8_t scrollPhase, uint8_t maxBri) {
    static constexpr uint8_t ringOffset[6] = { 0, 42, 85, 128, 170, 213 };
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

// Static: 6 fixed ring colors from ring_colors CSV field, no scrolling.
// ringColors format: "r,g,b;r,g,b;..." — 6 semicolon-separated RGB triples.
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

        // Parse "r,g,b" — look for two commas
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

// Blended: 6 rings blending with the gradient, similar to Spectrum but
// the gradient is user-chosen (RGB1→RGB2) and still scrolls.
static void renderBlended(const CRGB* gradient,
                          uint8_t scrollPhase, uint8_t maxBri) {
    static constexpr uint8_t ringOffset[6] = { 0, 42, 85, 128, 170, 213 };
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

// ─── Dispatch ────────────────────────────────────────────────────────────────

void updateRingShow(const LightShowParams& params,
                    const CRGB* gradient,
                    uint8_t colorPhase,
                    uint8_t maxBri) {
    const String& type = params.patternType;

    if (type.equalsIgnoreCase("Spectrum") || type.isEmpty()) {
        // Default: full-HSV rainbow scatter (backward-compatible with old Spectrum)
        // The caller has already filled gradient with 256 HSV entries
        renderSpectrum(gradient, colorPhase, maxBri);
    } else if (type.equalsIgnoreCase("Rainbow")) {
        // Rainbow: uses caller's gradient (RGB1→RGB2), spread across rings
        renderRainbow(gradient, colorPhase, maxBri);
    } else if (type.equalsIgnoreCase("Blended")) {
        // Blended: same layout as Rainbow, different conceptual name
        renderBlended(gradient, colorPhase, maxBri);
    } else if (type.equalsIgnoreCase("Static") && !params.ringColors.isEmpty()) {
        // Static: fixed colors per ring, no scrolling
        renderStaticRings(params.ringColors, maxBri);
    } else {
        // Fallback: Spectrum
        renderSpectrum(gradient, colorPhase, maxBri);
    }
}