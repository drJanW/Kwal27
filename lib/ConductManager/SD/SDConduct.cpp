/**
 * @file SDConduct.cpp
 * @brief SD card state management implementation
 * @version 251231E
 * @date 2025-12-31
 *
 * Stub implementation for SD conduct. SD card management is handled by
 * SDBoot (initialization) and SDPolicy (file operations).
 */

#include "SDConduct.h"

#include "Globals.h"

void SDConduct::plan() {
    // No periodic orchestration needed - SD managed via SDBoot + SDPolicy
}
