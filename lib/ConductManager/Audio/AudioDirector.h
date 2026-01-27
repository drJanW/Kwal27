/**
 * @file AudioDirector.h
 * @brief Audio fragment selection logic
 * @version 251231E
 * @date 2025-12-31
 *
 * Implements selection logic for audio fragment playback. Chooses which audio
 * fragments to play based on current context, theme boxes, and weighted random
 * selection from available SD card directories.
 */

#pragma once

#include <Arduino.h>
#include "PlayFragment.h"

class AudioDirector {
public:
    static void plan();

    // Select the next fragment to play based on current SD index/voting data.
    static bool selectRandomFragment(AudioFragment& outFrag);
};
