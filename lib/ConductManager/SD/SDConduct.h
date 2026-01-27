/**
 * @file SDConduct.h
 * @brief SD card state management
 * @version 251231E
 * @date 2025-12-31
 *
 * Manages SD card operational state. Currently a stub - SD card management
 * is handled via SDBoot and SDPolicy. No periodic orchestration needed.
 */

#pragma once

#include <Arduino.h>

class SDConduct {
public:
    void plan();
};
