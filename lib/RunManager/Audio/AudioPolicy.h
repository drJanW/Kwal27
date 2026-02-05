/**
 * @file AudioPolicy.h
 * @brief Audio playback business logic
 * @version 260202A
 * @date 2026-02-02
 */
#pragma once
#include <Arduino.h>
#include "AudioManager.h"
#include "PlayFragment.h"

namespace AudioPolicy {

    // Rule application
    float applyVolumeRules(float requested);

    // Distance-driven playback helpers
    bool distancePlaybackInterval(float distanceMm, uint32_t& intervalMs);
    float updateDistancePlaybackVolume(float distanceMm);

    // Optional: queueing policy
    bool requestFragment(const AudioFragment& frag);
    void requestSentence(const String& phrase);

    // Calendar-driven theme box support
    void setThemeBox(const uint8_t* dirs, size_t count, const String& id);
    void clearThemeBox();
    bool themeBoxActive();
    const uint8_t* themeBoxDirs(size_t& count);
    const String& themeBoxId();

    // Mix additional theme box directories into current pool
    // Returns number of dirs actually added (may be limited by capacity)
    size_t mergeThemeBoxDirs(const uint8_t* dirs, size_t count);

    // Reset to base theme box (removes any merged dirs)
    void resetToBaseThemeBox();
}
