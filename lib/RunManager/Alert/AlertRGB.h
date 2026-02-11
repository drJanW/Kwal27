/**
 * @file AlertRGB.h
 * @brief RGB LED failure flash coordination
 * @version 260131A
 $12026-02-05
 */
#pragma once

#include <stdint.h>

namespace AlertRGB {
    void startFlashing();  // Start failure flash bursts
    void stopFlashing();   // Stop and restore normal show
    bool isFlashing();     // True while flash cycle is active
}