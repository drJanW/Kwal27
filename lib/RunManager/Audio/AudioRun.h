/**
 * @file AudioRun.h
 * @brief Audio playback state management
 * @version 260212D
 * @date 2026-02-12
 */
#pragma once

#include <Arduino.h>

#include "AudioManager.h"
#include "TimerManager.h"

class AudioRun {
public:
    static const char* const distanceClipId;

    void plan();
    static void startDistanceResponse(bool playImmediately = false);
    static void cb_playPCM();
    static void cb_volumeShiftTimer();
};

void setDistanceClipPointer(const AudioManager::PCMClipDesc* clip);
const AudioManager::PCMClipDesc* getDistanceClipPointer();
