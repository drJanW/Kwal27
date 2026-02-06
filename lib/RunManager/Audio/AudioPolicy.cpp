/**
 * @file AudioPolicy.cpp
 * @brief Audio playback business logic implementation
 * @version 260131A
 * @date 2026-01-31
 */
#include "AudioPolicy.h"
#include "AudioState.h"  // isAudioBusy, isSentencePlaying
#include "PlaySentence.h"
#include "Globals.h" // only for constants like MAX_VOLUME

namespace {

constexpr uint32_t kIntervalMinMs = 160;
constexpr uint32_t kIntervalMaxMs = 2400;
constexpr float kIntervalMinMsF = static_cast<float>(kIntervalMinMs);
constexpr float kIntervalMaxMsF = static_cast<float>(kIntervalMaxMs);

constexpr size_t kMaxThemeDirs = MAX_THEME_DIRS;

float distanceVolume = 1.0f;  // Updated from Globals at runtime

bool    themeBoxIsActive = false;
uint8_t themeDirs[kMaxThemeDirs];
size_t  themeDirCount = 0;
String  themeId;

// Base theme box (before any merges)
uint8_t baseThemeDirs[kMaxThemeDirs];
size_t  baseThemeDirCount = 0;

} // namespace

namespace AudioPolicy {

float applyVolumeRules(float requested) {
    return clamp(requested, 0.0f, 1.0f);
}

bool requestFragment(const AudioFragment& frag) {
    if (audio.isPCMClipActive()) {
        audio.stopPCMClip();
    }
    // No arbitration check - caller (AudioRun) determines timing
    // PlaySentence handles graceful takeover if TTS starts
    return audio.startFragment(frag);
}

void requestSentence(const String& phrase) {
    // Route naar PlaySentence unified queue
    PlaySentence::addTTS(phrase.c_str());
}

void clearThemeBox() {
    if (!themeBoxIsActive && themeDirCount == 0 && themeId.isEmpty()) {
        return;
    }
    themeBoxIsActive = false;
    themeDirCount = 0;
    baseThemeDirCount = 0;  // Clear base too
    themeId = "";
    PF("[AudioPolicy] Theme box cleared\n");
}

void setThemeBox(const uint8_t* dirs, size_t count, const String& id) {
    if (!dirs || count == 0) {
        clearThemeBox();
        return;
    }

    const size_t limit = count > kMaxThemeDirs ? kMaxThemeDirs : count;
    for (size_t i = 0; i < limit; ++i) {
        themeDirs[i] = dirs[i];
        baseThemeDirs[i] = dirs[i];  // Save base copy
    }
    themeDirCount = limit;
    baseThemeDirCount = limit;  // Save base count
    themeBoxIsActive = themeDirCount > 0;
    themeId = id;

    PF_BOOT("[AudioPolicy] Box %s: %u dirs\n",
       themeId.c_str(), static_cast<unsigned>(themeDirCount));
}

bool themeBoxActive() {
    return themeBoxIsActive && themeDirCount > 0;
}

const uint8_t* themeBoxDirs(size_t& count) {
    count = themeDirCount;
    return themeBoxActive() ? themeDirs : nullptr;
}

const String& themeBoxId() {
    return themeId;
}

void resetToBaseThemeBox() {
    // Restore base theme box dirs (removes any merged additions)
    for (size_t i = 0; i < baseThemeDirCount; ++i) {
        themeDirs[i] = baseThemeDirs[i];
    }
    themeDirCount = baseThemeDirCount;
    themeBoxIsActive = themeDirCount > 0;
}

size_t mergeThemeBoxDirs(const uint8_t* dirs, size_t count) {
    if (!dirs || count == 0) {
        return 0;
    }
    
    size_t added = 0;
    for (size_t i = 0; i < count && themeDirCount < kMaxThemeDirs; ++i) {
        // Allow duplicates - they increase weight in random selection
        themeDirs[themeDirCount++] = dirs[i];
        ++added;
    }
    
    if (added > 0) {
        themeBoxIsActive = themeDirCount > 0;
        PF_BOOT("[AudioPolicy] +%u dirs (total=%u)\n",
           static_cast<unsigned>(added), static_cast<unsigned>(themeDirCount));
    }
    return added;
}

bool distancePlaybackInterval(float distanceMm, uint32_t& intervalMs) {
    // Silent if outside valid range
    if (distanceMm < Globals::distanceMinMm || distanceMm > Globals::distanceMaxMm) {
        return false;
    }

    const float mapped       = map(distanceMm,
                                   Globals::distanceMinMm,
                                   Globals::distanceMaxMm,
                                   kIntervalMinMsF,
                                   kIntervalMaxMsF);
    const float bounded      = clamp(mapped, kIntervalMinMsF, kIntervalMaxMsF);

    intervalMs = static_cast<uint32_t>(bounded + 0.5f);
    return true;
}

float updateDistancePlaybackVolume(float distanceMm) {
    if (distanceMm <= 0.0f) {
        return distanceVolume;
    }

    const float clampedDistance = clamp(distanceMm, Globals::distanceMinMm, Globals::distanceMaxMm);
    const float mapped = map(clampedDistance,
                             Globals::distanceMinMm,
                             Globals::distanceMaxMm,
                             Globals::pingVolumeMax,
                             Globals::pingVolumeMin);

    distanceVolume = clamp(mapped, Globals::pingVolumeMin, Globals::pingVolumeMax);
    return distanceVolume;
}

}
