/**
 * @file AudioState.cpp
 * @brief Thread-safe audio state storage using atomics
 * @version 260212C
 * @date 2026-02-12
 * 
 * All state is stored in std::atomic variables with relaxed ordering
 * for safe cross-core access on ESP32 dual-core architecture.
 */
#include "AudioState.h"
#include "Globals.h"
#include "MathUtils.h"

#include <atomic>

namespace {
std::atomic<float> g_volumeShiftedHi{0.37f};  // Hi boundary after shifts applied
std::atomic<float> g_volumeWebMultiplier{1.0f};     // User's web slider multiplier (can be >1.0)
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

float getVolumeWebMultiplier() {
    return g_volumeWebMultiplier.load(std::memory_order_relaxed);
}

void setVolumeWebMultiplier(float value) {
    // No clamp - can be >1.0 to compensate other shifts
    g_volumeWebMultiplier.store(value, std::memory_order_relaxed);
}

int getAudioSliderPct() {
    // effectiveHi = shiftedHi * webMultiplier (webMultiplier is separate multiplier)
    float shiftedHi = g_volumeShiftedHi.load(std::memory_order_relaxed);
    float webMultiplier = g_volumeWebMultiplier.load(std::memory_order_relaxed);
    float effectiveHi = shiftedHi * webMultiplier;
    // Map to slider percentage using Globals (like brightness)
    return static_cast<int>(MathUtils::mapRange(
        effectiveHi, Globals::volumeLo, Globals::volumeHi,
        Globals::loPct, Globals::hiPct));
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
