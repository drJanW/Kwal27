/**
 * @file AudioRun.h
 * @brief Audio playback state management
 * @version 260131A
 $12026-02-05
 */
#pragma once

#include <Arduino.h>

#include "AudioManager.h"
#include "TimerManager.h"

class AudioRun {
public:
    static const char* const kDistanceClipId;

    void plan();
    static void startDistanceResponse(bool playImmediately = false);
    static void cb_playPCM();
    static void cb_volumeShiftTimer();
};

void setDistanceClipPointer(const AudioManager::PCMClipDesc* clip);
const AudioManager::PCMClipDesc* getDistanceClipPointer();
