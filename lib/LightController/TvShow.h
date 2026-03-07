/**
 * @file TvShow.h
 * @brief TV simulator light renderer — 4 index-based color zones with smooth lerping
 * @version 260307A
 * @date 2026-03-07
 */
#pragma once

#include <FastLED.h>
#include "Globals.h"

#define TV_ZONES 6

struct TvZoneTarget {
    CRGB  color;
    uint8_t brightness;
};

// Ring boundaries: 6 concentric PMMA circles
// Ring 0: LEDs 0-7   (8 LEDs,  innermost)
// Ring 1: LEDs 8-23  (16 LEDs)
// Ring 2: LEDs 24-47 (24 LEDs)
// Ring 3: LEDs 48-79 (32 LEDs)
// Ring 4: LEDs 80-115 (36 LEDs)
// Ring 5: LEDs 116-159 (44 LEDs, outermost)

// Set new target colors/brightness for all zones (called by cb_tvScene)
void setTvZoneTargets(const TvZoneTarget targets[TV_ZONES]);

// Render one frame: lerp current → target, write to leds[], call FastLED.show()
void updateTvShow();
