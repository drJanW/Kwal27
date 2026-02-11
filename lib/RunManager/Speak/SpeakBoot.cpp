/**
 * @file SpeakBoot.cpp
 * @brief TTS speech one-time initialization implementation
 * @version 260131A
 $12026-02-05
 */
#include "SpeakBoot.h"
#include "Globals.h"
#include "SpeakPolicy.h"

void SpeakBoot::plan() {
    SpeakPolicy::configure();
}
