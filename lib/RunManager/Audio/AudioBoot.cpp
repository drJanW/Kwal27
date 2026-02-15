/**
 * @file AudioBoot.cpp
 * @brief Audio subsystem one-time initialization implementation
 * @version 260215F
 * @date 2026-02-15
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
#include "Speak/SpeakRun.h"
#include "PlaySentence.h"

void AudioBoot::plan() {
    if (!AlertState::isSdOk()) {
        PL_BOOT("[Run][Plan] Audio boot deferred: SD not ready");
        return;
    }

    audio.begin();
    hwStatus |= HW_AUDIO;
    
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

    AlertState::setAudioStatus(true);
    PlaySentence::speakNext();  // Kickstart queue if items waiting
}
