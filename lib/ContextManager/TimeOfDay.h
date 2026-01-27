/**
 * @file TimeOfDay.h
 * @brief Time-of-day period detection interface
 * @version 251231E
 * @date 2025-12-31
 *
 * Declares the TimeOfDay namespace with functions for detecting
 * the current time-of-day period. Provides bitmask generation for
 * active time status flags and individual convenience functions
 * for checking specific periods (night, dawn, morning, etc.).
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
