/**
 * @file StatusBoot.h
 * @brief Status display one-time initialization
 * @version 260204A
 * @date 2026-02-04
 */
#pragma once

// Recurring timer callback for status/time display
void cb_timeDisplay();

class StatusBoot {
public:
    void plan();
};

extern StatusBoot statusBoot;
