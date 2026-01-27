/**
 * @file SpeakPolicy.cpp
 * @brief TTS speech business logic implementation
 * @version 251231E
 * @date 2025-12-31
 *
 * Implements speech arbitration: speech always has priority over fragments,
 * only blocks if another sentence is already playing. PlaySentence handles
 * fragment interruption automatically.
 */

#include "SpeakPolicy.h"
#include "Globals.h"
#include "AudioState.h"

namespace SpeakPolicy {

void configure() {
    // No configuration needed yet
}

bool canSpeak() {
    // Speak heeft ALTIJD prioriteit
    // PlaySentence::addWords() stopt fragment indien nodig
    // Alleen blokkeren als er al een sentence speelt
    if (isSentencePlaying()) {
        return false;
    }
    return true;
}

}
