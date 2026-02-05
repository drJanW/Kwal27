/**
 * @file AlertRun.h
 * @brief Hardware failure alert state management
 * @version 260205A
 * @date 2026-02-05
 */
#pragma once

#include "AlertRequest.h"
#include "AlertState.h"  // for StatusComponent

/**
 * @class AlertRun
 * @brief Orchestrates hardware failure detection and alert sequences
 * 
 * Manages flash bursts and voice announcements when hardware components fail.
 */
class AlertRun {
public:
    /// Register alert timers with TimerManager
    static void plan();
    
    /// Report hardware status change (updates AlertState)
    static void report(AlertRequest request);
    
    /// React to status: on LAST_TRY → set FAILED + speak
    static void speakOnFail(StatusComponent c);
};