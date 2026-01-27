/**
 * @file SensorsPolicy.h
 * @brief Sensor data processing business logic
 * @version 251231E
 * @date 2025-12-31
 *
 * Contains business logic for sensor data processing. Normalizes raw VL53L1X
 * distance readings, filters invalid values, and tracks freshness window
 * for distance data.
 */

#pragma once

#include <Arduino.h>

namespace SensorsPolicy {

void configure();

// Normalise raw VL53 distance; returns true when value accepted.
bool normaliseDistance(float rawMm, uint32_t sampleTsMs, float& filteredOut);

// Lightweight accessor for the most recent distance value.
float currentDistance();

// Retrieve newest distance (only if still within new window).
bool newestDistance(float& distanceMm);

// Fast mode: rapid polling when movement detected
bool isFastModeActive();
uint32_t getPollingIntervalMs();
void exitFastMode();

// Internal: called by normaliseDistance to check delta
void cb_exitFastMode();

// Distance cleared announcement cooldown
bool canSpeakDistanceCleared();

}
