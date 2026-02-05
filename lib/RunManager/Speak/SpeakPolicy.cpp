/**
 * @file SpeakPolicy.cpp
 * @brief TTS speech business logic implementation
 * @version 260205A
 * @date 2026-02-05
 */
#include "SpeakPolicy.h"
#include "Globals.h"
#include "AudioState.h"

namespace SpeakPolicy {

void configure() {
    // No configuration needed yet
}

/// Check if speech can be started
/// Speech ALWAYS has priority - PlaySentence::addWords() stops fragment if needed
/// Only block if a sentence is already playing
bool canSpeak() {
    if (isSentencePlaying()) {
        return false;
    }
    return true;
}

}
