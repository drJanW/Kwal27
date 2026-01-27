/**
 * @file AudioConduct.h
 * @brief Audio playback state management
 * @version 251231E
 * @date 2025-12-31
 *
 * Manages audio playback state and coordination. Handles distance-responsive
 * playback timing, volume shift timers, and PCM clip timer creation. Coordinates
 * with AudioPolicy for playback decisions and AudioDirector for clip selection.
 */

#pragma once

#include <Arduino.h>

#include "AudioManager.h"
#include "TimerManager.h"

class AudioConduct {
public:
    static const char* const kDistanceClipId;

    void plan();
    static void startDistanceResponse(bool playImmediately = false);
    static void cb_playPCM();
    static void cb_volumeShiftTimer();
};

void setDistanceClipPointer(const AudioManager::PCMClipDesc* clip);
const AudioManager::PCMClipDesc* getDistanceClipPointer();
