/**
 * @file HeartbeatPolicy.h
 * @brief Heartbeat LED business logic
 * @version 260131A
 * @date 2026-01-31
 */
#pragma once

#include <Arduino.h>

#ifndef HEARTBEAT_DEBUG
#define HEARTBEAT_DEBUG 0
#endif

namespace HeartbeatPolicy {

// Prepare internal state (smoothing, defaults).
void configure();

// Return the default interval used when no sensor data is available.
uint32_t defaultIntervalMs();

// Clamp an interval to the supported range.
uint32_t clampInterval(uint32_t intervalMs);

// Bootstrap policy state with an initial distance; returns false if distance invalid.
bool bootstrap(float distanceMm, uint32_t &intervalOut);

// Update policy with a new raw distance. Returns true iff the heartbeat interval should change.
bool intervalFromDistance(float distanceMm, uint32_t &intervalOut);

} // namespace HeartbeatPolicy
