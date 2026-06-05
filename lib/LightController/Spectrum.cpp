/**
 * @file Spectrum.cpp
 * @brief Pattern #32 — 6 ring renderer
 * @version 260604D
 * @date 2026-06-04
 */
#include <Arduino.h>
#include "Spectrum.h"

extern CRGB leds[];

// PMMA ring boundaries — identical to TvShow ringStart layout
static constexpr uint8_t ringStart[7] = { 0, 8, 24, 48, 80, 116, 160 };

// 6 evenly-spaced sample offsets across the 256-entry gradient
// (256 / 6 ≈ 42.67; chosen multiples of ~42 give clean separation)
static constexpr uint8_t ringOffset[6] = { 0, 42, 85, 128, 170, 213 };

void renderSpectrum(const CRGB* gradient, uint8_t scrollPhase, uint8_t maxBri) {
    for (uint8_t z = 0; z < 6; z++) {
        uint8_t idx = static_cast<uint8_t>(scrollPhase + ringOffset[z]);  // 256-wrap
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
