/**
 * @file AlertRun.h
 * @brief Hardware failure alert state management
 * @version 251231E
 * @date 2025-12-31
 *
 * Runs hardware alert coordination. Receives component
 * status reports (OK/FAIL), updates AlertState, and triggers appropriate
 * RGB flash patterns and TTS announcements for failures.
 */

#pragma once

#include "AlertRequest.h"
#include "AlertState.h"  // voor StatusComponent

class AlertRun {
public:
    static void plan();
    static void report(AlertRequest request);
    
    // Reageer op status: bij LAST_TRY → set FAILED + speak
    static void speakOnFail(StatusComponent c);
};