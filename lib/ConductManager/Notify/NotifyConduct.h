/**
 * @file NotifyConduct.h
 * @brief Hardware failure notification state management
 * @version 251231E
 * @date 2025-12-31
 *
 * Manages hardware notification state and coordination. Receives component
 * status reports (OK/FAIL), updates NotifyState, and triggers appropriate
 * RGB flash patterns and TTS announcements for failures.
 */

#pragma once

#include "NotifyIntent.h"
#include "NotifyState.h"  // voor StatusComponent

class NotifyConduct {
public:
    static void plan();
    static void report(NotifyIntent intent);
    
    // Reageer op status: bij LAST_TRY → set FAILED + speak
    static void speakOnFail(StatusComponent c);
};
