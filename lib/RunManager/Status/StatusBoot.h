/**
 * @file StatusBoot.h
 * @brief Status display one-time initialization
 * @version 251231E
 * @date 2025-12-31
 *
 * Handles status display subsystem initialization during boot. Configures
 * status policy and provides time display callback. Part of the Boot→Plan→
 * Policy→Run pattern for status display coordination.
 */

#pragma once

// Recurring timer callback for status/time display
void cb_timeDisplay();

class StatusBoot {
public:
    void plan();
};

extern StatusBoot statusBoot;
