/**
 * @file SensorsPolicy.cpp
 * @brief Sensor data policy implementation
 * @version 260202A
 $12026-02-05
 */
#include "SensorsPolicy.h"
#include "Globals.h"
#include "SensorController.h"
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

// Fast interval state
bool fastIntervalActive = false;
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
    fastIntervalActive = false;
    previousDistanceMm = 0.0f;
    havePreviousDistance = false;
    distanceClearedCooldownActive = false;
    SensorController::setDistanceMillimeters(lastDistanceMm);
    SP_LOG("[SensorsPolicy] Reset distance filter state\n");
}

void cb_exitFastInterval() {
    fastIntervalActive = false;
    PF("[SensorsPolicy] Fast interval ended\n");
}

bool isFastIntervalActive() {
    return fastIntervalActive;
}

uint32_t getPollingIntervalMs() {
    return fastIntervalActive ? Globals::sensorFastIntervalMs : Globals::sensorBaseDefaultMs;
}

void exitFastInterval() {
    fastIntervalActive = false;
    timers.cancel(cb_exitFastInterval);
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

    // Fast interval trigger: check delta from previous reading
    if (havePreviousDistance) {
        float delta = std::fabs(filtered - previousDistanceMm);
        if (delta >= Globals::sensorFastDeltaMm) {
            if (!fastIntervalActive) {
                fastIntervalActive = true;
                PF("[SensorsPolicy] Fast interval triggered (delta=%.1fmm)\n", delta);
            }
            // Reset/extend fast interval duration timer
            timers.restart(Globals::sensorFastDurationMs, 1, cb_exitFastInterval);
        }
    }
    previousDistanceMm = filtered;
    havePreviousDistance = true;

    lastDistanceMm = filtered;
    haveDistance = true;
    distanceIsNew = true;
    SensorController::setDistanceMillimeters(filtered);
    filteredOut = filtered;

    timers.restart(Globals::distanceNewWindowMs, 1, cb_distanceOld);

    SP_LOG("[SensorsPolicy] raw=%.1f filtered=%.1f fast=%d\n",
           rawMm,
           filtered,
           fastIntervalActive ? 1 : 0);

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
    timers.create(DISTANCE_CLEARED_COOLDOWN_MS, 1, cb_distanceClearedCooldownEnd);
    return true;
}

} // namespace SensorsPolicy
