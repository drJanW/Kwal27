/**
 * @file SpeakPolicy.h
 * @brief TTS speech business logic
 * @version 251231E
 * @date 2025-12-31
 *
 * Contains business logic for TTS speech decisions. Determines when speech
 * is allowed based on current audio state (sentences have priority over
 * fragments).
 */

#pragma once

namespace SpeakPolicy {

void configure();
bool canSpeak();

}
