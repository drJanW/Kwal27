/**
 * @file RunManager.h
 * @brief Central coordinator header for all Kwal modules
 * @version 260407C
 * @date 2026-04-07
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
    static void requestSetDemoVolume();  // max volume for demo, no web expiry
    static void requestSetAudioIntervals(
        uint32_t speakMinMs, uint32_t speakMaxMs, bool hasSpeakRange,
        uint32_t fragMinMs,  uint32_t fragMaxMs,  bool hasFragRange,
        bool silence, uint32_t durationMs);
    static void requestSetSilence(bool active);
    static void requestStopAudio();
    static void touchCalActivity();
    static bool requestStartClockTick(bool fallbackEnabled);
    static bool isClockRunning();
    static bool isClockInFallback();
    static bool requestSeedClockFromRtc();
    static void requestSyncRtcFromClock();

    // Lux measurement request (Run-compliant)
    static void requestLuxMeasurement();

    // Start periodic audio timers (called by BootSequencer verdict)
    static void armAudioTimers();

    // Legacy boot handoffs — now no-ops (sequencer handles this)
    static void resumeAfterSDBoot();
    static void resumeAfterWiFiBoot();

    // TV Simulator
    static void enterTvMode(uint8_t hours);
    static void exitTvMode();

    // Free-text TTS
    static void requestSetWebFreeTextTts(const String& text, uint32_t intervalMs, uint8_t repeatCount,
                                         int8_t voiceIndex = -1, int8_t tempo = 99, uint8_t volumePct = 0);
    static void requestClearWebFreeTextTts();
    static const String& getWebFreeTextTtsText();

    // Deep Sleep
    static void requestDeepSleep();
    static void cancelDeepSleep();
};
