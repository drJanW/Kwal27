/**
 * @file AudioBoot.cpp
 * @brief Audio subsystem one-time initialization implementation
 * @version 251231E
 * @date 2025-12-31
 *
 * Implements audio boot sequence: initializes AudioManager hardware interface,
 * loads audio shift parameters from SD card CSV, and reports audio hardware
 * status to the notification system.
 */

#include "AudioBoot.h"

#include "Globals.h"
#include "AudioManager.h"
#include "PlayPCM.h"
#include "AudioRun.h"
#include "AudioShiftTable.h"
#include "Alert/AlertState.h"
#include "Speak/SpeakRun.h"
#include "PlaySentence.h"

void AudioBoot::plan() {
    if (!AlertState::isSdOk()) {
        PL("[Run][Plan] Audio boot deferred: SD not ready");
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
        PL("[Run][Plan] Distance ping clip unavailable");
    }

    AlertState::setAudioStatus(true);
    PlaySentence::speakNext();  // Kickstart queue if items waiting
    PL("[Run][Plan] AudioManager initialized");
}
