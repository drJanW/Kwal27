/**
 * @file StatusFlags.h
 * @brief Hardware failure bits and status tracking interface
 * @version 260212A
 * @date 2026-02-12
 */
#pragma once

#include <stdint.h>

// Unified status flag computation
// Combines: TimeOfDay, Season, Weekday, Weather, MoonPhase
namespace StatusFlags {

// Get complete status bitmask with all active flags
// This combines time-of-day, season, weekday, weather, and moon phase
uint64_t getFullStatusBits();

// Individual category getters (for debugging/display)
uint64_t getTimeOfDayBits();    // Night, Dawn, Morning, etc.
uint64_t getSeasonBits();       // Spring, Summer, Autumn, Winter
uint64_t getWeekdayBits();      // Monday-Sunday, Weekend
uint64_t getWeatherBits();      // Freezing, Cold, Mild, Warm, Hot
uint64_t getMoonPhaseBits();    // NewMoon, Waxing, FullMoon, Waning
uint64_t getHardwareFailBits(); // SD, WiFi, RTC, NTP, sensors
uint64_t getTemperatureShiftBits();
float getTemperatureSwing();

} // namespace StatusFlags