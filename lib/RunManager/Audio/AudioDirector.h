/**
 * @file AudioDirector.h
 * @brief Audio fragment selection logic
 * @version 260306G
 * @date 2026-03-06
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
