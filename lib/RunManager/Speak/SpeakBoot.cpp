/**
 * @file SpeakBoot.cpp
 * @brief TTS speech one-time initialization implementation
 * @version 260131A
 * @date 2026-01-31
 */
#include "SpeakBoot.h"
#include "Globals.h"
#include "SpeakPolicy.h"

void SpeakBoot::plan() {
    SpeakPolicy::configure();
}
