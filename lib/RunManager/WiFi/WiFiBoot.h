/**
 * @file WiFiBoot.h
 * @brief WiFi connection one-time initialization
 * @version 251231E
 * @date 2025-12-31
 *
 * Handles WiFi subsystem initialization during boot. Coordinates WiFi
 * connection, NTP time fetch, and module initialization sequencing.
 * Part of the Boot→Plan→Policy→Run pattern for WiFi coordination.
 */

#pragma once

#include <Arduino.h>

class WiFiBoot {
public:
    void plan();
};
