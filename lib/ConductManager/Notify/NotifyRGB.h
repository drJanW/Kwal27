/**
 * @file NotifyRGB.h
 * @brief RGB LED failure flash coordination
 * @version 251231E
 * @date 2025-12-31
 *
 * Manages RGB LED failure indication patterns. Flashes component-specific
 * colors when hardware failures are detected, temporarily overriding the
 * normal light show during failure notification sequences.
 */

#pragma once

#include <stdint.h>

namespace NotifyRGB {
    void startFlashing();  // Start failure flash bursts
    void stopFlashing();   // Stop and restore normal show
    bool isFlashing();     // True while flash cycle is active
}
