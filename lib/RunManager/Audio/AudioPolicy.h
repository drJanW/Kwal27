/**
 * @file AudioPolicy.h
 * @brief Audio playback business logic
 * @version 260226A
 * @date 2026-02-26
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
    size_t mergeThemeBoxDirs(const uint8_t* dirs, size_t count);

    // Reset to base theme box (removes any merged dirs)
    void resetToBaseThemeBox();

    // Web silence (temporary mute from WebGUI)
    bool isWebSilenceActive();
    void setWebSilence(bool active);

    // Web speak interval (temporary from WebGUI)
    void setWebSpeakRange(uint32_t minMs, uint32_t maxMs);
    void clearWebSpeakRange();
    uint32_t effectiveSpeakMin();
    uint32_t effectiveSpeakMax();
    uint32_t webSpeakCenterMin();   // center in minutes for SSE round-trip

    // Web fragment interval (temporary from WebGUI)
    void setWebFragmentRange(uint32_t minMs, uint32_t maxMs);
    void clearWebFragmentRange();
    uint32_t effectiveFragmentMin();
    uint32_t effectiveFragmentMax();
    bool isWebFragmentRangeActive();
    uint32_t webFragCenterMin();    // center in minutes for SSE round-trip
}
