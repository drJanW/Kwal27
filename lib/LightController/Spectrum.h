/**
 * @file Spectrum.h
 * @brief Pattern #32 — 6 concentric rings, each a distinct color, slowly rotating through the gradient
 * @version 260604D
 * @date 2026-06-04
 *
 * Reuses ring zone boundaries from TvShow (PMMA circle layout) and the
 * colorGradient[256] table built by the main renderer. No new timers,
 * no extra state — scrolls via the existing colorPhase tick.
 */
#pragma once

#include <FastLED.h>

/// Render one Spectrum frame to leds[].
/// @param gradient    256-entry color gradient (RGB1 → RGB2)
/// @param scrollPhase 0..255, advances via cb_colorCycle
/// @param maxBri      brightness scale (0..255, typically getBrightnessBaseHi())
void renderSpectrum(const CRGB* gradient, uint8_t scrollPhase, uint8_t maxBri);
