/**
 * @file TimeOfDay.h
 * @brief Time-of-day period detection interface
 * @version 260202A
 * @date 2026-02-02
 */
#pragma once

#include <stdint.h>

namespace TimeOfDay {

// Returns a bitmask of currently active time-of-day status flags
// Each bit corresponds to a TimeStatus enum value
uint64_t getActiveStatusBits();

// Individual flag checkers (convenience functions)
bool isNight();
bool isDawn();
bool isMorning();
bool isLight();
bool isDay();
bool isAfternoon();
bool isDusk();
bool isEvening();
bool isDark();
bool isAM();
bool isPM();

} // namespace TimeOfDay
