/**
 * @file SpeakBoot.cpp
 * @brief TTS speech one-time initialization implementation
 * @version 251231E
 * @date 2025-12-31
 *
 * Implements speech boot sequence: configures SpeakPolicy for sentence
 * playback rules and priority handling.
 */

#include "SpeakBoot.h"
#include "Globals.h"
#include "SpeakPolicy.h"

void SpeakBoot::plan() {
    SpeakPolicy::configure();
}
