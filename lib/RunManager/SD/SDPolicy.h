/**
 * @file SDPolicy.h
 * @brief SD card file operation business logic
 * @version 260131A
 * @date 2026-01-31
 */
#pragma once
#include <Arduino.h>
#include "PlayFragment.h"   // for AudioFragment

namespace SDPolicy {

    // Weighted random fragment selection
    bool getRandomFragment(AudioFragment& outFrag);

    // Delete a file (only if allowed by policy)
    bool deleteFile(uint8_t dirIndex, uint8_t fileIndex);

    // Diagnostics
    void showStatus(bool forceLog = false);
}
