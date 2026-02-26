/**
 * @file SpeakPolicy.cpp
 * @brief TTS speech business logic implementation
 * @version 260226A
 * @date 2026-02-26
 */
#include "SpeakPolicy.h"
#include "Globals.h"
#include "AudioState.h"
#include "Audio/AudioPolicy.h"

namespace SpeakPolicy {

void configure() {
    // No configuration needed yet
}

/// Check if speech can be started
/// Speech ALWAYS has priority - PlaySentence::addWords() stops fragment if needed
/// Only block if a sentence is already playing or web silence is active
bool canSpeak() {
    if (isSentencePlaying()) {
        return false;
    }
    if (AudioPolicy::isWebSilenceActive()) {
        return false;
    }
    return true;
}

}
