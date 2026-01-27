/**
 * @file PlayFragment.cpp
 * @brief Audio fragment playback with fade-in/fade-out support
 * @version 251231E
 * @date 2025-12-31
 *
 * Implements audio fragment playback from SD card MP3 files.
 * Supports configurable fade-in and fade-out transitions using a power curve.
 * Handles fragment timing, start position offset, and duration limits.
 * Provides smooth volume transitions to avoid audio pops and clicks.
 */

#include "PlayFragment.h"
#include "Globals.h"
#include "AudioState.h"
#include "AudioManager.h"
#include "TimerManager.h"
#include "SDManager.h"
#include "WebGuiStatus.h"
#include <math.h>

extern const char* getMP3Path(uint8_t dirIdx, uint8_t fileIdx);

#ifndef FADE_POWER
#define FADE_POWER 2.0f
#endif

namespace {

constexpr uint8_t kFadeSteps = 15;

struct FadeState {
    bool     curveReady = false;
    float    curve[kFadeSteps];
    uint16_t effectiveMs = 0;
    uint16_t stepMs = 1;
    uint32_t fadeOutDelayMs = 0;
    uint8_t  inIndex = 0;
    uint8_t  outIndex = 0;
    uint8_t  lastCurveIndex = 0;
    float    currentFactor = 0.0f;
};

FadeState& fade() {
    static FadeState state;
    return state;
}

AudioManager& audio() {
    return AudioManager::instance();
}

void initCurve() {
    auto& state = fade();
    if (state.curveReady) return;
    float power = FADE_POWER;
    if (power < 1.0f) {
        power = 1.0f;
    }
    for (uint8_t i = 0; i < kFadeSteps; ++i) {
        const float numerator = static_cast<float>(i);
        uint8_t stepCount = 1U;
        if (kFadeSteps > 1U) {
            stepCount = static_cast<uint8_t>(kFadeSteps - 1U);
        }
        const float denominator = static_cast<float>(stepCount);
        float x = 1.0f;
        if (kFadeSteps > 1U) {
            x = numerator / denominator;
        }
        const float s = sinf(1.5707963f * x);
        state.curve[i] = powf(s, power);
    }
    state.curveReady = true;
}

inline void setFadeFactor(float value) {
    fade().currentFactor = value;
}

inline float fadeFactor() {
    return fade().currentFactor;
}

inline float currentGain() {
    return getVolumeShiftedHi() * fadeFactor() * getVolumeWebShift();
}

inline void applyGain() {
    audio().audioOutput.SetGain(currentGain());
}

void resetFadeIndices() {
    fade().inIndex = 0;
    fade().outIndex = 0;
    fade().lastCurveIndex = 0;
    fade().currentFactor = 0.0f;
}

void stopPlayback();
void cb_fadeIn();
void cb_fadeOut();
void cb_beginFadeOut();
void cb_fragmentReady();

} // namespace

namespace PlayAudioFragment {

bool start(const AudioFragment& fragment) {
    if (isAudioBusy()) {
        LOG_WARN("[Audio] startFragment ignored: audio busy\n");
        return false;
    }

    initCurve();

    auto& state = fade();

    SDManager::setSDbusy(true);  // SD busy while streaming MP3
    setAudioBusy(true);

    uint32_t maxFade = 1;
    if (fragment.durationMs >= 2) {
        maxFade = fragment.durationMs / 2;
    }
    uint32_t requested = fragment.fadeMs;
    // Enforce minimum fade of 500ms for audible fade-out
    constexpr uint32_t kMinFadeMs = 500;
    if (requested < kMinFadeMs) requested = kMinFadeMs;
    if (requested > maxFade) requested = maxFade;
    state.effectiveMs = static_cast<uint16_t>(requested);
    if (state.effectiveMs == 0) {
        state.stepMs = 1;
    } else {
        state.stepMs = static_cast<uint16_t>(state.effectiveMs / kFadeSteps);
    }
    if (state.stepMs == 0) state.stepMs = 1;
    if (fragment.durationMs > static_cast<uint32_t>(state.effectiveMs) * 2U) {
        state.fadeOutDelayMs = fragment.durationMs - static_cast<uint32_t>(state.effectiveMs) * 2U;
    } else {
        state.fadeOutDelayMs = 0;
    }
    resetFadeIndices();

    setFadeFactor(0.0f);
    applyGain();

    audio().audioFile = new AudioFileSourceSD(getMP3Path(fragment.dirIndex, fragment.fileIndex));
    if (!audio().audioFile) {
        LOG_ERROR("[Audio] Failed to allocate source for %03u/%03u\n", fragment.dirIndex, fragment.fileIndex);
        stopPlayback();
        return false;
    }

    audio().audioMp3Decoder = new AudioGeneratorMP3();
    if (!audio().audioMp3Decoder) {
        LOG_ERROR("[Audio] Failed to allocate MP3 decoder\n");
        stopPlayback();
        return false;
    }

    if (!audio().audioMp3Decoder->begin(audio().audioFile, &audio().audioOutput)) {
        LOG_ERROR("[Audio] Decoder begin failed for %03u/%03u\n", fragment.dirIndex, fragment.fileIndex);
        stopPlayback();
        return false;
    }

    // Playback is now guaranteed to have started; only now publish current fragment.
    setSentencePlaying(false);
    setFragmentPlaying(true);
    setCurrentDirFile(fragment.dirIndex, fragment.fileIndex, fragment.score);
    WebGuiStatus::setFragment(fragment.dirIndex, fragment.fileIndex, fragment.score, fragment.durationMs);

    timers.cancel(cb_beginFadeOut);
    timers.cancel(cb_fadeIn);
    timers.cancel(cb_fadeOut);

    if (!timers.create(state.stepMs, kFadeSteps, cb_fadeIn)) {
        LOG_WARN("[Fade] Failed to start fade-in timer\n");
    }

    if (state.fadeOutDelayMs == 0) {
        if (!timers.create(state.stepMs, kFadeSteps, cb_fadeOut)) {
            LOG_WARN("[Fade] Failed to start fade-out timer\n");
        }
    } else {
        if (!timers.create(state.fadeOutDelayMs, 1, cb_beginFadeOut)) {
            LOG_WARN("[Fade] Failed to create fade-out delay (%lu ms)\n", static_cast<unsigned long>(state.fadeOutDelayMs));
        }
    }

    // Timer-based completion (T4 rule: never use loop() return for completion)
    timers.cancel(cb_fragmentReady);
    if (!timers.create(fragment.durationMs, 1, cb_fragmentReady)) {
        LOG_WARN("[Audio] Failed to create fragment completion timer\n");
    }

    PF("[audio][fragment] %02u - %02u playing (fade=%ums gain=%.2f)\n", 
       fragment.dirIndex, fragment.fileIndex, state.effectiveMs,
       static_cast<double>(getVolumeShiftedHi() * getVolumeWebShift()));

    return true;
}

constexpr uint16_t kFadeUseCurrent = 0xFFFF;
constexpr uint16_t kFadeMinMs = 40;

void stop(uint16_t fadeOutMs) {
    if (!isAudioBusy()) return;

    auto& state = fade();

    timers.cancel(cb_fragmentReady);
    timers.cancel(cb_beginFadeOut);
    timers.cancel(cb_fadeOut);
    timers.cancel(cb_fadeIn);

    uint16_t effective = fadeOutMs;
    if (effective == kFadeUseCurrent) {
        effective = state.effectiveMs;
    }

    if (effective <= kFadeMinMs || kFadeSteps == 0U) {
        stopPlayback();
        return;
    }

    state.effectiveMs = effective;
    state.fadeOutDelayMs = 0;
    state.stepMs = static_cast<uint16_t>(effective / kFadeSteps);
    if (state.stepMs == 0) {
        state.stepMs = 1;
    }

    uint8_t startOffset = 0;
    if (kFadeSteps > 0U) {
        startOffset = static_cast<uint8_t>((kFadeSteps - 1U) - state.lastCurveIndex);
    }
    state.outIndex = startOffset;

    if (!timers.create(state.stepMs, kFadeSteps, cb_fadeOut)) {
        LOG_WARN("[Fade] Failed to create stop() fade-out timer\n");
        stopPlayback();
    }
}

void updateGain() {
    if (!isAudioBusy()) return;
    applyGain();
}

} // namespace PlayAudioFragment

namespace {

void stopPlayback() {
    timers.cancel(cb_fragmentReady);
    timers.cancel(cb_beginFadeOut);
    timers.cancel(cb_fadeIn);
    timers.cancel(cb_fadeOut);

    if (audio().audioMp3Decoder) {
        audio().audioMp3Decoder->stop();
        delete audio().audioMp3Decoder;
        audio().audioMp3Decoder = nullptr;
    }
    if (audio().audioFile) {
        delete audio().audioFile;
        audio().audioFile = nullptr;
    }

    setFadeFactor(0.0f);
    applyGain();

    SDManager::setSDbusy(false);  // SD no longer busy
    setAudioBusy(false);
    setFragmentPlaying(false);
    setSentencePlaying(false);

    fade().effectiveMs = 0;
    fade().stepMs = 1;
    fade().fadeOutDelayMs = 0;
    resetFadeIndices();

    audio().updateGain();
}

void cb_fadeIn() {
    auto& state = fade();
    uint8_t idx = 0;
    if (kFadeSteps > 0U) {
        idx = static_cast<uint8_t>(kFadeSteps - 1U);
    }
    if (state.inIndex < kFadeSteps) {
        idx = state.inIndex;
    }
    setFadeFactor(state.curve[idx]);
    applyGain();
    state.lastCurveIndex = idx;

    state.inIndex++;
    if (state.inIndex >= kFadeSteps) {
        timers.cancel(cb_fadeIn);
        state.inIndex = 0;
    }
}

void cb_fadeOut() {
    auto& state = fade();
    uint8_t idx = 0;
    if (state.outIndex < kFadeSteps) {
        idx = static_cast<uint8_t>((kFadeSteps - 1U) - state.outIndex);
    }
    setFadeFactor(state.curve[idx]);
    applyGain();
    state.lastCurveIndex = idx;

    state.outIndex++;
    if (state.outIndex >= kFadeSteps) {
        timers.cancel(cb_fadeOut);
        state.outIndex = 0;
        stopPlayback();
    }
}

void cb_beginFadeOut() {
    auto& state = fade();
    uint8_t startOffset = 0;
    if (kFadeSteps > 0U) {
        startOffset = static_cast<uint8_t>((kFadeSteps - 1U) - state.lastCurveIndex);
    }
    state.outIndex = startOffset;
    timers.cancel(cb_fadeOut);
    uint16_t step = state.stepMs;
    if (step == 0) {
        step = 1;
    }
    if (!timers.create(step, kFadeSteps, cb_fadeOut)) {
        LOG_WARN("[Fade] Failed to launch delayed fade-out timer\n");
        stopPlayback();
    }
}

void cb_fragmentReady() {
    PF("[Audio] Fragment completed via timer\n");
    stopPlayback();
}

} // namespace

void PlayAudioFragment::abortImmediate() {
    stopPlayback();
}
