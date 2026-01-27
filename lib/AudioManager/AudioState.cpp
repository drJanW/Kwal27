/**
 * @file AudioState.cpp
 * @brief Shared audio state between playback modules
 * @version 251231E
 * @date 2025-12-31
 *
 * Implements thread-safe audio state management using atomic variables.
 * Tracks playback status (fragment, sentence, TTS, word playing states).
 * Manages audio gain levels (base gain, web audio level, raw level).
 */

#include "AudioState.h"
#include "Globals.h"
#include "MathUtils.h"

#include <atomic>

namespace {
std::atomic<float> g_volumeShiftedHi{0.37f};  // Hi boundary after shifts applied
std::atomic<float> g_volumeWebShift{1.0f};     // User's web slider multiplier (can be >1.0)
std::atomic<int16_t> g_audioLevelRaw{0};
std::atomic<bool> g_audioBusy{false};
std::atomic<uint8_t> g_currentDir{0};
std::atomic<uint8_t> g_currentFile{0};
std::atomic<uint8_t> g_currentScore{0};
std::atomic<bool> g_currentValid{false};
std::atomic<bool> g_fragmentPlaying{false};
std::atomic<bool> g_sentencePlaying{false};
std::atomic<bool> g_ttsActive{false};
std::atomic<bool> g_wordPlaying{false};
std::atomic<int32_t> g_currentWordId{0};
} // namespace

bool isTtsActive() {
    return g_ttsActive.load(std::memory_order_relaxed);
}

float getVolumeWebShift() {
    return g_volumeWebShift.load(std::memory_order_relaxed);
}

void setVolumeWebShift(float value) {
    // No clamp - can be >1.0 to compensate other shifts
    g_volumeWebShift.store(value, std::memory_order_relaxed);
}

int getAudioSliderPct() {
    // effectiveHi = shiftedHi * webShift (webShift is separate multiplier)
    float shiftedHi = g_volumeShiftedHi.load(std::memory_order_relaxed);
    float webShift = g_volumeWebShift.load(std::memory_order_relaxed);
    float effectiveHi = shiftedHi * webShift;
    // Map to slider percentage using Globals (like brightness)
    return static_cast<int>(MathUtils::mapRange(
        effectiveHi,
        Globals::volumeLo, Globals::volumeHi,
        static_cast<float>(Globals::loPct), static_cast<float>(Globals::hiPct)));
}

void setAudioLevelRaw(int16_t value) {
    g_audioLevelRaw.store(value, std::memory_order_relaxed);
}

int16_t getAudioLevelRaw() {
    return g_audioLevelRaw.load(std::memory_order_relaxed);
}

float getVolumeShiftedHi() {
    return g_volumeShiftedHi.load(std::memory_order_relaxed);
}

void setVolumeShiftedHi(float value) {
    g_volumeShiftedHi.store(value, std::memory_order_relaxed);
}

bool isAudioBusy() {
    return g_audioBusy.load(std::memory_order_relaxed);
}

void setAudioBusy(bool value) {
    g_audioBusy.store(value, std::memory_order_relaxed);
}

bool getCurrentDirFile(uint8_t& dir, uint8_t& file, uint8_t& score) {
    if (!g_currentValid.load(std::memory_order_relaxed)) {
        return false;
    }
    dir = g_currentDir.load(std::memory_order_relaxed);
    file = g_currentFile.load(std::memory_order_relaxed);
    score = g_currentScore.load(std::memory_order_relaxed);
    return true;
}

void setCurrentDirFile(uint8_t dir, uint8_t file, uint8_t score) {
    g_currentDir.store(dir, std::memory_order_relaxed);
    g_currentFile.store(file, std::memory_order_relaxed);
    g_currentScore.store(score, std::memory_order_relaxed);
    g_currentValid.store(true, std::memory_order_relaxed);
}

bool isFragmentPlaying() {
    return g_fragmentPlaying.load(std::memory_order_relaxed);
}

void setFragmentPlaying(bool value) {
    g_fragmentPlaying.store(value, std::memory_order_relaxed);
}

bool isSentencePlaying() {
    return g_sentencePlaying.load(std::memory_order_relaxed);
}

void setSentencePlaying(bool value) {
    g_sentencePlaying.store(value, std::memory_order_relaxed);
}

void setTtsActive(bool value) {
    g_ttsActive.store(value, std::memory_order_relaxed);
}

void setWordPlaying(bool value) {
    g_wordPlaying.store(value, std::memory_order_relaxed);
}

void setCurrentWordId(int32_t value) {
    g_currentWordId.store(value, std::memory_order_relaxed);
}
