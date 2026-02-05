/**
 * @file StatusBoot.h
 * @brief Status display one-time initialization
 * @version 260131A
 * @date 2026-01-31
 */
#pragma once

// Recurring timer callback for status/time display
void cb_timeDisplay();

class StatusBoot {
public:
    void plan();
};

extern StatusBoot statusBoot;
