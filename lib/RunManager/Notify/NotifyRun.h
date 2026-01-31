/**
 * @file NotifyRun.h
 * @brief Hardware failure notification state management
 * @version 251231E
 * @date 2025-12-31
 *
 * Runs hardware notification coordination. Receives component
 * status reports (OK/FAIL), updates NotifyState, and triggers appropriate
 * RGB flash patterns and TTS announcements for failures.
 */

#pragma once

#include "NotifyIntent.h"
#include "NotifyState.h"  // voor StatusComponent

class NotifyRun {
public:
    static void plan();
    static void report(NotifyIntent intent);
    
    // Reageer op status: bij LAST_TRY → set FAILED + speak
    static void speakOnFail(StatusComponent c);
};
