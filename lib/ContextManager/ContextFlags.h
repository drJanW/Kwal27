/**
 * @file ContextFlags.h
 * @brief Hardware failure bits and status tracking interface
 * @version 251231E
 * @date 2025-12-31
 *
 * Declares the ContextFlags namespace with functions for computing
 * unified context bitmasks. Combines time-of-day, season, weekday,
 * weather, and moon phase into a single bitmask for context-aware
 * decision making throughout the application.
 */

#pragma once

#include <stdint.h>

// Unified context flag computation
// Combines: TimeOfDay, Season, Weekday, Weather, MoonPhase
namespace ContextFlags {

// Get complete context bitmask with all active flags
// This combines time-of-day, season, weekday, weather, and moon phase
uint64_t getFullContextBits();

// Individual category getters (for debugging/display)
uint64_t getTimeOfDayBits();    // Night, Dawn, Morning, etc.
uint64_t getSeasonBits();       // Spring, Summer, Autumn, Winter
uint64_t getWeekdayBits();      // Monday-Sunday, Weekend
uint64_t getWeatherBits();      // Freezing, Cold, Mild, Warm, Hot
uint64_t getMoonPhaseBits();    // NewMoon, Waxing, FullMoon, Waning
uint64_t getHardwareFailBits(); // SD, WiFi, RTC, NTP, sensors

} // namespace ContextFlags
