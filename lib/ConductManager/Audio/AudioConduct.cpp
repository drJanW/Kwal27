/**
 * @file AudioConduct.cpp
 * @brief Audio playback state management implementation
 * @version 251231E
 * @date 2025-12-31
 *
 * Implements audio playback orchestration: manages PCM playback timers,
 * handles distance-triggered audio responses, coordinates volume shifts
 * based on context, and schedules fragment playback through AudioPolicy.
 */

#include "AudioConduct.h"

#include <Arduino.h>
#include <atomic>

#include "AudioManager.h"
#include "AudioState.h"
#include "PlayPCM.h"
#include "PlayFragment.h"
#include "AudioPolicy.h"
#include "AudioShiftStore.h"
#include "ContextFlags.h"
#include "Globals.h"
#include "TimerManager.h"
#include "Sensors/SensorsPolicy.h"

#ifndef AUDIO_CONDUCT_DEBUG
#define AUDIO_CONDUCT_DEBUG 0
#endif

#if AUDIO_CONDUCT_DEBUG
#define AC_LOG(...) PF(__VA_ARGS__)
#else
#define AC_LOG(...) \
    do              \
    {               \
    } while (0)
#endif

const char *const AudioConduct::kDistanceClipId = "distance_ping";

namespace
{

    constexpr uint32_t volumeShiftCheckMs = 60000;

    std::atomic<const AudioManager::PCMClipDesc *> distanceClipPtr{nullptr};
    uint64_t lastStatusBits = 0;

    AudioManager &audio()
    {
        return AudioManager::instance();
    }

    void applyVolumeShift(uint64_t statusBits)
    {
        float effectiveVolume = AudioShiftStore::instance().getEffectiveVolume(statusBits);
        
        float scaledVolume = effectiveVolume * MAX_VOLUME;
        if (scaledVolume < 0.0f) scaledVolume = 0.0f;
        if (scaledVolume > MAX_VOLUME) scaledVolume = MAX_VOLUME;
        
        setVolumeShiftedHi(scaledVolume);
        PF("[AudioConduct] Volume shift: %.2f (eff=%.2f, status=0x%llX)\n",
           static_cast<double>(scaledVolume), static_cast<double>(effectiveVolume), statusBits);
    }

    bool attemptDistancePlayback()
    {
        const auto *clip = getDistanceClipPointer();
        if (!clip)
        {
            PF("[AudioConduct] Distance PCM clip missing, cancel playback attempt\n");
            return false;
        }

        const float distanceMm = SensorsPolicy::currentDistance();

        // Check if policy allows playback at this distance
        uint32_t unused = 0;
        if (!AudioPolicy::distancePlaybackInterval(distanceMm, unused))
        {
            return false;  // Distance out of range, don't play
        }
        const float volumeMultiplier = AudioPolicy::updateDistancePlaybackVolume(distanceMm);
        const float pcmVolume = clamp(Globals::basePlaybackVolume * volumeMultiplier,
                                      Globals::minDistanceVolume,
                                      1.0f);

        AC_LOG("[AudioConduct] Triggering distance PCM (distance=%.1fmm, volume=%.2f)\n",
               static_cast<double>(distanceMm),
               static_cast<double>(pcmVolume));
        if (!PlayPCM::play(clip, pcmVolume))
        {
            PF("[AudioConduct] Failed to start distance PCM playback\n");
        }

        return true;
    }

} // namespace

// boot registers the clip once; later calls simply overwrite the atomic pointer
void setDistanceClipPointer(const AudioManager::PCMClipDesc *clip)
{
    setMux(clip, &distanceClipPtr);
}

// call sites expect a valid clip because startDistanceResponse refuses to arm otherwise
const AudioManager::PCMClipDesc *getDistanceClipPointer()
{
    return getMux(&distanceClipPtr);
}

void AudioConduct::cb_playPCM()
{
    if (!attemptDistancePlayback())
    {
        return;
    }

    startDistanceResponse();
}

void AudioConduct::cb_volumeShiftTimer()
{
    uint64_t statusBits = ContextFlags::getFullContextBits();
    if (statusBits != lastStatusBits) {
        lastStatusBits = statusBits;
        applyVolumeShift(statusBits);
    }
    
    timers.create(volumeShiftCheckMs, 1, AudioConduct::cb_volumeShiftTimer);
}

void AudioConduct::plan()
{
    timers.cancel(AudioConduct::cb_playPCM);
    timers.cancel(AudioConduct::cb_volumeShiftTimer);
    
    // Apply initial volume shift and start periodic timer
    lastStatusBits = ContextFlags::getFullContextBits();
    applyVolumeShift(lastStatusBits);
    timers.create(volumeShiftCheckMs, 1, AudioConduct::cb_volumeShiftTimer);
    
    PF("[Conduct][Plan] Distance playback ready with clip %s\n", kDistanceClipId);
}

void AudioConduct::startDistanceResponse(bool playImmediately)
{
    // if boot never set the clip we skip scheduling entirely
    if (!getDistanceClipPointer())
    {
        return;
    }

    const float distanceMm = SensorsPolicy::currentDistance();

    uint32_t intervalMs = 0;
    const bool policyAllowsPlayback = AudioPolicy::distancePlaybackInterval(distanceMm, intervalMs);
    if (!policyAllowsPlayback)
    {
        intervalMs = 1000 * 60 * 60 * 1000U; // park the timer far in the future when policy declines
    }

    auto &tm = timers;  // global TimerManager

    auto &mgr = audio();
    // fragments fade out before distance pings; stop using existing fade behaviour
    if (isFragmentPlaying())
    {
        const uint16_t fadeMs = static_cast<uint16_t>(clamp(intervalMs, 100U, 5000U));
        PlayAudioFragment::stop(fadeMs);
    }

    if (policyAllowsPlayback && playImmediately)
    {
        if (!attemptDistancePlayback())
        {
            return;
        }
    }

    AC_LOG("[AudioConduct] Distance response scheduled (distance=%.1fmm, interval=%lu ms)\n",
           static_cast<double>(distanceMm),
           static_cast<unsigned long>(intervalMs));

    // Use restart() - distance triggers can happen repeatedly, reschedule if pending
    if (!tm.restart(intervalMs, 1, AudioConduct::cb_playPCM))
    {
        PF("[AudioConduct] Failed to schedule distance playback (%lu ms)\n",
           static_cast<unsigned long>(intervalMs));
    }
}

// no need for additional helpers; pointer is wired once during boot
