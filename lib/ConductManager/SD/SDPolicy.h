/**
 * @file SDPolicy.h
 * @brief SD card file operation business logic
 * @version 251231E
 * @date 2025-12-31
 *
 * Contains business logic for SD card file operations. Provides weighted
 * random fragment selection via AudioDirector, file deletion policy with
 * audio-busy checks, and diagnostic status reporting.
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
