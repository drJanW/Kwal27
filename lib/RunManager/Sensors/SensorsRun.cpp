/**
 * @file SensorsRun.cpp
 * @brief Sensor data processing state management implementation
 * @version 251231E
 * @date 2025-12-31
 *
 * Implements sensor event processing: reads distance events from SensorManager,
 * applies normalization via SensorsPolicy, and triggers heartbeat rate changes,
 * audio playback, and light animation updates based on filtered distance.
 */

#include "SensorsRun.h"

#include "Globals.h"
#include "SensorManager.h"
#include "SensorsPolicy.h"
#include "TimerManager.h"

#include "Heartbeat/HeartbeatRun.h"
#include "Heartbeat/HeartbeatPolicy.h"
#include "Audio/AudioRun.h"
#include "Audio/AudioPolicy.h"
#include "Light/LightRun.h"
#include "Speak/SpeakRun.h"

namespace {

constexpr uint8_t SENSOR_EVENT_DISTANCE = 0x30;

bool distancePlaybackEligible = false;

void cb_processSensorEvents() {
    SensorEvent ev;

    while (SensorManager::readEvent(ev)) {
        if (ev.type != SENSOR_EVENT_DISTANCE) {
            continue;
        }

        float distanceMm = static_cast<float>(ev.value);

        // Check raw distance against audio playback range FIRST
        // If outside range, cancel ping timer immediately
        uint32_t dummy = 0;
        if (!AudioPolicy::distancePlaybackInterval(distanceMm, dummy) && distancePlaybackEligible) {
            distancePlaybackEligible = false;
            timers.cancel(AudioRun::cb_playPCM);
        }

        float filteredMm = 0.0f;
        if (!SensorsPolicy::normaliseDistance(distanceMm, ev.ts_ms, filteredMm)) {
            continue;
        }

        distanceMm = filteredMm;
        if (distanceMm <= 0.0f) {
            continue;
        }

        uint32_t interval = 0;
        if (HeartbeatPolicy::intervalFromDistance(distanceMm, interval)) {
            heartbeatRun.setRate(interval);
        }

        const bool audioEligible = AudioPolicy::distancePlaybackInterval(distanceMm, dummy);

        if (audioEligible) {
            if (!distancePlaybackEligible) {
                distancePlaybackEligible = true;
                AudioRun::startDistanceResponse(true);
            }
        } else if (distancePlaybackEligible) {
            distancePlaybackEligible = false;
            timers.cancel(AudioRun::cb_playPCM);
            // Object moved away - speak "geen afstand" if cooldown allows
            if (SensorsPolicy::canSpeakDistanceCleared()) {
                SpeakRun::speak(SpeakRequest::DISTANCE_CLEARED);
            }
        }

        LightRun::updateDistance(distanceMm);
    }

    // Reschedule with current interval (fast or normal)
    timers.restart(SensorsPolicy::getPollingIntervalMs(), 0, cb_processSensorEvents);
}

} // namespace

void SensorsRun::plan() {
    distancePlaybackEligible = false;
    timers.restart(SensorsPolicy::getPollingIntervalMs(), 0, cb_processSensorEvents);
    PF("[Run][Plan] Sensor processing scheduled\n");
}
