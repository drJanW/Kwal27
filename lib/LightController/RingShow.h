/**
 * @file RingShow.h
 * @brief Ring renderer dispatch for pattern #32 — multiple ring visualisation types
 * @version 260614B
 * @date 2026-06-14
 *
 * Replaces the old Spectrum module with a cleaner, extensible ring-renderer
 * system.  Pattern #32 entries in light_patterns.csv carry a pattern_type
 * field and a ring_colors list.  The dispatch selects the renderer by type;
 * each renderer reads only the CSV columns it needs.
 */
#pragma once

#include <FastLED.h>
#include "Globals.h"
#include "LightController.h"

/// Ring show entry point: fill leds[] for pattern #32.
/// @param params      Current LightShowParams (CSV columns available)
/// @param gradient    256-entry color gradient (RGB1→RGB2) —
///                    pre-filled by the caller; renderers may ignore.
/// @param colorPhase  Scroll phase 0..255 (advances via cb_colorCycle)
/// @param maxBri      Brightness scale (0..255)
void updateRingShow(const LightShowParams& params,
                    const CRGB* gradient,
                    uint8_t colorPhase,
                    uint8_t maxBri);