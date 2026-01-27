/**
 * @file SensorsPolicy.cpp
 * @brief Sensor data processing business logic implementation
 * @version 251231E
 * @date 2025-12-31
 *
 * Implements sensor data normalization: validates distance range (40-3600mm),
 * tracks freshness with automatic expiration timer, and provides smoothed
 * distance values for downstream consumers.
 */

#include "SensorsPolicy.h"
#include "Globals.h"
#include "SensorManager.h"
#include "TimerManager.h"

#include <cmath>

namespace {

#ifndef SENSORS_POLICY_DEBUG
#define SENSORS_POLICY_DEBUG 0
#endif

#if SENSORS_POLICY_DEBUG
#define SP_LOG(...) PF(__VA_ARGS__)
#else
#define SP_LOG(...) do {} while (0)
#endif

bool haveDistance = false;
bool distanceIsNew = false;
float lastDistanceMm = 0.0f;

// Fast mode state
bool fastModeActive = false;
float previousDistanceMm = 0.0f;
bool havePreviousDistance = false;

// Distance cleared cooldown state
bool distanceClearedCooldownActive = false;
constexpr uint32_t DISTANCE_CLEARED_COOLDOWN_MS = 10000;  // 10 seconds between announcements

void cb_distanceOld() {
    distanceIsNew = false;
}

void cb_distanceClearedCooldownEnd() {
    distanceClearedCooldownActive = false;
}

} // namespace

namespace SensorsPolicy {

void configure() {
    haveDistance = false;
    distanceIsNew = false;
    lastDistanceMm = 0.0f;
    fastModeActive = false;
    previousDistanceMm = 0.0f;
    havePreviousDistance = false;
    distanceClearedCooldownActive = false;
    SensorManager::setDistanceMillimeters(lastDistanceMm);
    SP_LOG("[SensorsPolicy] Reset distance filter state\n");
}

void cb_exitFastMode() {
    fastModeActive = false;
    PF("[SensorsPolicy] Fast mode ended\n");
}

bool isFastModeActive() {
    return fastModeActive;
}

uint32_t getPollingIntervalMs() {
    return fastModeActive ? Globals::sensorFastIntervalMs : Globals::sensorBaseDefaultMs;
}

void exitFastMode() {
    fastModeActive = false;
    TimerManager::instance().cancel(cb_exitFastMode);
}

bool normaliseDistance(float rawMm, uint32_t sampleTsMs, float& filteredOut) {
    (void)sampleTsMs;
    if (!std::isfinite(rawMm)) {
        return false;
    }
    if (rawMm < Globals::distanceMinMm || rawMm > Globals::distanceMaxMm) {
        return false;
    }

    const float filtered = rawMm;

    // Fast mode trigger: check delta from previous reading
    if (havePreviousDistance) {
        float delta = std::fabs(filtered - previousDistanceMm);
        if (delta >= Globals::sensorFastDeltaMm) {
            if (!fastModeActive) {
                fastModeActive = true;
                PF("[SensorsPolicy] Fast mode triggered (delta=%.1fmm)\n", delta);
            }
            // Reset/extend fast mode duration timer
            TimerManager::instance().restart(Globals::sensorFastDurationMs, 1, cb_exitFastMode);
        }
    }
    previousDistanceMm = filtered;
    havePreviousDistance = true;

    lastDistanceMm = filtered;
    haveDistance = true;
    distanceIsNew = true;
    SensorManager::setDistanceMillimeters(filtered);
    filteredOut = filtered;

    TimerManager::instance().restart(Globals::distanceNewWindowMs, 1, cb_distanceOld);

    SP_LOG("[SensorsPolicy] raw=%.1f filtered=%.1f fast=%d\n",
           rawMm,
           filtered,
           fastModeActive ? 1 : 0);

    return true;
}

float currentDistance() {
    return lastDistanceMm;
}

bool newestDistance(float& distanceMm) {
    if (!haveDistance) {
        return false;
    }
    if (!distanceIsNew) {
        return false;
    }
    distanceMm = lastDistanceMm;
    return true;
}

bool canSpeakDistanceCleared() {
    if (distanceClearedCooldownActive) {
        return false;
    }
    distanceClearedCooldownActive = true;
    TimerManager::instance().create(DISTANCE_CLEARED_COOLDOWN_MS, 1, cb_distanceClearedCooldownEnd);
    return true;
}

} // namespace SensorsPolicy
