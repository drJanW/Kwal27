/**
 * @file LightBoot.h
 * @brief LED show one-time initialization
 * @version 251231E
 * @date 2025-12-31
 *
 * Handles LED subsystem initialization during boot. Configures FastLED,
 * loads LED mapping from SD card, and starts update timers. Part of the
 * Boot→Plan→Policy→Run pattern for LED show coordination.
 */

#pragma once

class LightBoot {
public:
    void plan();
};
