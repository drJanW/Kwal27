/**
 * @file ConductManager.h
 * @brief Central coordinator header for all Kwal modules
 * @version 251231E
 * @date 2025-12-31
 *
 * Defines the ConductManager class which serves as the main coordination layer.
 * Routes intents from the web interface to appropriate modules, manages
 * system lifecycle, and provides status queries for clock, OTA, and audio state.
 * All module coordination follows the Boot→Plan→Policy→Conduct pattern.
 */

#pragma once
#include <Arduino.h>
#include "AudioManager.h"
#include "LightManager.h"
#include "SDManager.h"
#include "OTAManager.h"
#include "TimerManager.h"
#include "PRTClock.h"


// High-level orchestrator of system behavior
class ConductManager {

public:
    // Lifecycle
    static void begin();
    static void update();

    // Intents (external requests)
    static void intentPlayFragment();
    static void intentPlaySpecificFragment(uint8_t dir, int8_t file);  // file=-1 for random from dir
    static void intentWebAudioNext(uint16_t fadeMs);
    static void triggerBootFragment();  // Called by CalendarConduct after theme box set
    static void intentSayTime(TimeStyle style = TimeStyle::NORMAL);
    static void intentSetAudioLevel(float value);
    static void intentArmOTA(uint32_t window_s);
    static bool intentConfirmOTA();
    static void intentShowTimerStatus();
    static bool intentStartClockTick(bool fallbackMode);
    static bool isClockRunning();
    static bool isClockInFallback();
    static bool intentSeedClockFromRtc();
    static void intentSyncRtcFromClock();

    // Lux measurement intent (Conduct-compliant)
    static void requestLuxMeasurement();

private:
    friend class SDBoot;
    // Internal helpers
    static void resumeAfterSDBoot();
};
