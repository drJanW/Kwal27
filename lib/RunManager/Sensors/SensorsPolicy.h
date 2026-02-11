/**
 * @file SensorsPolicy.h
 * @brief Sensor data policy
 * @version 260202A
 $12026-02-05
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

// Fast interval: rapid polling when movement detected
bool isFastIntervalActive();
uint32_t getPollingIntervalMs();
void exitFastInterval();

// Internal: called by normaliseDistance to check delta
void cb_exitFastInterval();

// Distance cleared announcement cooldown
bool canSpeakDistanceCleared();

}
