/**
 * @file PlayFragment.cpp
 * @brief MP3 fragment playback with sine-power fade curves
 * @version 260205A
 * @date 2026-02-05
 * 
 * Implements fade-in/fade-out using shared Globals::fadeCurve (sine² curve).
 * Timer-driven: no polling, no loop() dependency.
 * 
 * Fade curve: Globals::fadeCurve[i] = sin²(π/2 × i/(N-1)), precomputed at boot.
 */
#include "PlayFragment.h"
#include "Globals.h"
#include "AudioState.h"
#include "AudioManager.h"
#include "TimerManager.h"
#include "SDController.h"
#include "WebGuiStatus.h"

extern const char* getMP3Path(uint8_t dirIdx, uint8_t fileIdx);

namespace {

struct FadeState {
    uint16_t effectiveMs = 0;
    uint16_t stepMs = 1;
    uint32_t fadeOutDelayMs = 0;
    uint8_t  inIndex = 0;
    uint8_t  outIndex = 0;
    uint8_t  lastCurveIndex = 0;
    float    currentFraction = 0.0f;
};

FadeState& fade() {
    static FadeState state;
    return state;
}

inline void setFadeFraction(float value) {
    fade().currentFraction = value;
}

inline float fadeFraction() {
    return fade().currentFraction;
}

inline float currentVolumeMultiplier() {
    return getVolumeShiftedHi() * fadeFraction() * getVolumeWebShift();
}

inline void applyVolume() {
    audio.audioOutput.SetGain(currentVolumeMultiplier());
}

void resetFadeIndices() {
    fade().inIndex = 0;
    fade().outIndex = 0;
    fade().lastCurveIndex = 0;
    fade().currentFraction = 0.0f;
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
        return false;
    }

    auto& state = fade();

    SDController::lockSD();  // SD busy while streaming MP3
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
        state.stepMs = static_cast<uint16_t>(state.effectiveMs / Globals::fadeStepCount);
    }
    if (state.stepMs == 0) state.stepMs = 1;
    if (fragment.durationMs > static_cast<uint32_t>(state.effectiveMs) * 2U) {
        state.fadeOutDelayMs = fragment.durationMs - static_cast<uint32_t>(state.effectiveMs) * 2U;
    } else {
        state.fadeOutDelayMs = 0;
    }
    resetFadeIndices();

    setFadeFraction(0.0f);
    applyVolume();

    audio.audioFile = new AudioFileSourceSD(getMP3Path(fragment.dirIndex, fragment.fileIndex));
    if (!audio.audioFile) {
        LOG_ERROR("[Audio] Failed to allocate source for %03u/%03u\n", fragment.dirIndex, fragment.fileIndex);
        stopPlayback();
        return false;
    }

    audio.audioMp3Decoder = new AudioGeneratorMP3();
    if (!audio.audioMp3Decoder) {
        LOG_ERROR("[Audio] Failed to allocate MP3 decoder\n");
        stopPlayback();
        return false;
    }

    if (!audio.audioMp3Decoder->begin(audio.audioFile, &audio.audioOutput)) {
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

    if (!timers.create(state.stepMs, Globals::fadeStepCount, cb_fadeIn)) {
        LOG_WARN("[Fade] Failed to start fade-in timer\n");
    }

    if (state.fadeOutDelayMs == 0) {
        if (!timers.create(state.stepMs, Globals::fadeStepCount, cb_fadeOut)) {
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

    PF("[audio][fragment] %02u - %02u playing (fade=%ums volume=%.2f)\n", 
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

    if (effective <= kFadeMinMs || Globals::fadeStepCount == 0U) {
        stopPlayback();
        return;
    }

    state.effectiveMs = effective;
    state.fadeOutDelayMs = 0;
    state.stepMs = static_cast<uint16_t>(effective / Globals::fadeStepCount);
    if (state.stepMs == 0) {
        state.stepMs = 1;
    }

    uint8_t startOffset = 0;
    if (Globals::fadeStepCount > 0U) {
        startOffset = static_cast<uint8_t>((Globals::fadeStepCount - 1U) - state.lastCurveIndex);
    }
    state.outIndex = startOffset;

    if (!timers.create(state.stepMs, Globals::fadeStepCount, cb_fadeOut)) {
        LOG_WARN("[Fade] Failed to create stop() fade-out timer\n");
        stopPlayback();
    }
}

void updateVolume() {
    if (!isAudioBusy()) return;
    applyVolume();
}

} // namespace PlayAudioFragment

namespace {

void stopPlayback() {
    timers.cancel(cb_fragmentReady);
    timers.cancel(cb_beginFadeOut);
    timers.cancel(cb_fadeIn);
    timers.cancel(cb_fadeOut);

    if (audio.audioMp3Decoder) {
        audio.audioMp3Decoder->stop();
        delete audio.audioMp3Decoder;
        audio.audioMp3Decoder = nullptr;
    }
    if (audio.audioFile) {
        delete audio.audioFile;
        audio.audioFile = nullptr;
    }

    setFadeFraction(0.0f);
    applyVolume();

    SDController::unlockSD();  // SD no longer busy
    setAudioBusy(false);
    setFragmentPlaying(false);
    setSentencePlaying(false);

    fade().effectiveMs = 0;
    fade().stepMs = 1;
    fade().fadeOutDelayMs = 0;
    resetFadeIndices();

    audio.updateVolume();
}

void cb_fadeIn() {
    auto& state = fade();
    uint8_t idx = 0;
    if (Globals::fadeStepCount > 0U) {
        idx = static_cast<uint8_t>(Globals::fadeStepCount - 1U);
    }
    if (state.inIndex < Globals::fadeStepCount) {
        idx = state.inIndex;
    }
    setFadeFraction(Globals::fadeCurve[idx]);
    applyVolume();
    state.lastCurveIndex = idx;

    state.inIndex++;
    if (state.inIndex >= Globals::fadeStepCount) {
        timers.cancel(cb_fadeIn);
        state.inIndex = 0;
    }
}

void cb_fadeOut() {
    auto& state = fade();
    uint8_t idx = 0;
    if (state.outIndex < Globals::fadeStepCount) {
        idx = static_cast<uint8_t>((Globals::fadeStepCount - 1U) - state.outIndex);
    }
    setFadeFraction(Globals::fadeCurve[idx]);
    applyVolume();
    state.lastCurveIndex = idx;

    state.outIndex++;
    if (state.outIndex >= Globals::fadeStepCount) {
        timers.cancel(cb_fadeOut);
        state.outIndex = 0;
        stopPlayback();
    }
}

void cb_beginFadeOut() {
    auto& state = fade();
    uint8_t startOffset = 0;
    if (Globals::fadeStepCount > 0U) {
        startOffset = static_cast<uint8_t>((Globals::fadeStepCount - 1U) - state.lastCurveIndex);
    }
    state.outIndex = startOffset;
    timers.cancel(cb_fadeOut);
    uint16_t step = state.stepMs;
    if (step == 0) {
        step = 1;
    }
    if (!timers.create(step, Globals::fadeStepCount, cb_fadeOut)) {
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
