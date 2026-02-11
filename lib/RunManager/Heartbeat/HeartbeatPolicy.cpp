/**
 * @file HeartbeatPolicy.cpp
 * @brief Heartbeat LED business logic implementation
 * @version 260131A
 $12026-02-09
 */
#include "HeartbeatPolicy.h"

#include "Globals.h"
#include <math.h>

namespace {

#if HEARTBEAT_DEBUG
#define HB_LOG(...) PF(__VA_ARGS__)
#else
#define HB_LOG(...) do {} while (0)
#endif

constexpr uint32_t HEARTBEAT_JITTER_MS = 10;    // minimum delta before updating interval

uint32_t distanceToHeartbeat(float mm) {
	float clamped = clamp(mm, Globals::distanceMinMm, Globals::distanceMaxMm);
	float mapped = map(clamped,
		Globals::distanceMinMm, Globals::distanceMaxMm,
		Globals::heartbeatMinMs, Globals::heartbeatMaxMs);
	float bounded = clamp(mapped, Globals::heartbeatMinMs, Globals::heartbeatMaxMs);
	return static_cast<uint32_t>(bounded + 0.5f);
}

uint32_t lastHeartbeatMs = 0;

} // namespace

namespace HeartbeatPolicy {

void configure() {
	lastHeartbeatMs = 0;
}

uint32_t defaultIntervalMs() {
	return Globals::heartbeatDefaultMs;
}

uint32_t clampInterval(uint32_t intervalMs) {
	if (intervalMs < Globals::heartbeatMinMs) return Globals::heartbeatMinMs;
	if (intervalMs > Globals::heartbeatMaxMs) return Globals::heartbeatMaxMs;
	return intervalMs;
}

bool bootstrap(float distanceMm, uint32_t &intervalOut) {
	if (distanceMm <= 0.0f) {
		return false;
	}

	uint32_t next = distanceToHeartbeat(distanceMm);
	lastHeartbeatMs = next;
	intervalOut = next;
	HB_LOG("[HeartbeatPolicy] Bootstrap distance %.0fmm -> %lums\n",
		   distanceMm,
		   static_cast<unsigned long>(intervalOut));
	return true;
}

bool intervalFromDistance(float distanceMm, uint32_t &intervalOut) {
	if (distanceMm <= 0.0f) {
		return false;
	}

	uint32_t interval = distanceToHeartbeat(distanceMm);
	if (lastHeartbeatMs != 0) {
		const uint32_t diff = (interval > lastHeartbeatMs)
		    ? (interval - lastHeartbeatMs)
		    : (lastHeartbeatMs - interval);
		if (diff < HEARTBEAT_JITTER_MS) {
			return false;
		}
	}

	lastHeartbeatMs = interval;
	intervalOut = interval;
	HB_LOG("[HeartbeatPolicy] Distance %.0fmm -> %lums\n",
	   distanceMm,
	   static_cast<unsigned long>(interval));
	return true;
}

} // namespace HeartbeatPolicy
