/**
 * @file SDRun.cpp
 * @brief SD card state management implementation
 * @version 251231E
 * @date 2025-12-31
 *
 * Stub implementation for SD run. SD card management is handled by
 * SDBoot (initialization) and SDPolicy (file operations).
 */

#include "SDRun.h"

#include "Globals.h"

void SDRun::plan() {
    // No periodic orchestration needed - SD managed via SDBoot + SDPolicy
}
