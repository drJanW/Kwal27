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
#include "SDManager.h"
#include "PlayPCM.h"
#include "AudioConduct.h"
#include "AudioShiftStore.h"
#include "Notify/NotifyState.h"
#include "Speak/SpeakConduct.h"
#include "PlaySentence.h"

void AudioBoot::plan() {
    if (!SDManager::isReady()) {
        PL("[Conduct][Plan] Audio boot deferred: SD not ready");
        return;
    }

    audio.begin();
    hwStatus |= HW_AUDIO;
    
    // Initialize audio shift store
    AudioShiftStore::instance().begin();

    if (auto* clip = PlayPCM::loadFromSD("/ping.wav")) {
        setDistanceClipPointer(clip);
        AudioConduct::startDistanceResponse();
    } else {
        PL("[Conduct][Plan] Distance ping clip unavailable");
    }

    NotifyState::setAudioStatus(true);
    PlaySentence::speakNext();  // Kickstart queue if items waiting
    PL("[Conduct][Plan] Audio manager initialized");
}
