/**
 * @file RunManager.h
 * @brief Central coordinator header for all Kwal modules
 * @version 260219E
 * @date 2026-02-19
 */
#pragma once
#include <Arduino.h>
#include "AudioManager.h"
#include "LightController.h"
#include "SDController.h"
#include "TimerManager.h"
#include "PRTClock.h"


// High-level run coordinator of system behavior
class RunManager {

public:
    // Lifecycle
    static void begin();
    static void update();

    // Requests (external inputs)
    static void requestPlayFragment(const char* source = "timer");
    static void requestPlaySpecificFragment(uint8_t dir, int8_t file, const char* source = "?");
    static void requestSetSingleDirThemeBox(uint8_t dir);
    static void requestWebAudioNext(uint16_t fadeMs);
    static void requestStartSync();
    static void requestStopSync();
    static void triggerBootFragment();  // Called by CalendarRun after theme box set
    static void requestSayTime(TimeStyle style = TimeStyle::NORMAL);
    static void requestSayRTCtemperature();
    static void requestSetAudioLevel(float value);
    static bool requestStartClockTick(bool fallbackEnabled);
    static bool isClockRunning();
    static bool isClockInFallback();
    static bool requestSeedClockFromRtc();
    static void requestSyncRtcFromClock();

    // Lux measurement request (Run-compliant)
    static void requestLuxMeasurement();

    // Called by WiFiBoot after CSV fetch completes or times out
    static void resumeAfterWiFiBoot();

private:
    friend class SDBoot;
    // Internal helpers
    static void resumeAfterSDBoot();
};
