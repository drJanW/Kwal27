/**
 * @file AudioBoot.cpp
 * @brief Audio subsystem one-time initialization implementation
 * @version 260218K
 * @date 2026-02-18
 */
#include "AudioBoot.h"

#include "Globals.h"
#include "MathUtils.h"
#include "AudioManager.h"
#include "AudioState.h"
#include "PlayPCM.h"
#include "AudioRun.h"
#include "AudioShiftTable.h"
#include "Alert/AlertState.h"
#include "Alert/AlertRun.h"
#include "Speak/SpeakRun.h"
#include "PlaySentence.h"

void AudioBoot::plan() {
    // I2S audio init — always needed (TTS uses network, not SD)
    audio.begin();
    hwStatus |= HW_AUDIO;
    AlertState::setAudioStatus(true);

    if (!AlertState::isSdOk()) {
        PL("[AudioBoot] SD absent — TTS only mode");
        // SD_FAIL was reported before audio was ready — speak it now
        SpeakRun::speakFail(SC_SD);
        // Welcome was queued but never played (CalendarRun gate) — play now
        AlertRun::playWelcomeIfPending();
        return;
    }

    // Initialize audio shift table
    AudioShiftTable::instance().begin();

    if (auto* clip = PlayPCM::loadFromSD("/ping.wav")) {
        setDistanceClipPointer(clip);
        AudioRun::startDistanceResponse();
    } else {
        PL_BOOT("[Run][Plan] Distance ping clip unavailable");
    }

    // Compute initial volumeWebMultiplier from defaultAudioSliderPct (Globals/CSV).
    // Maps desired slider% to target volume, then divides by MAX_VOLUME
    // to get approximate multiplier. First applyVolumeShift() will refine shiftedHi,
    // after which the slider will show approximately the configured percentage.
    float targetVol = MathUtils::mapRange(
        Globals::defaultAudioSliderPct,
        Globals::loPct, Globals::hiPct,
        Globals::volumeLo, Globals::volumeHi);
    float initVolMult = (MAX_VOLUME > 0.0f)
        ? (targetVol / MAX_VOLUME)
        : 1.0f;
    setVolumeWebMultiplier(initVolMult);
    PF("[AudioBoot] defaultAudioSliderPct=%u → volumeWebMultiplier=%.3f\n",
       Globals::defaultAudioSliderPct, initVolMult);

    PlaySentence::speakNext();  // Kickstart queue if items waiting
}
