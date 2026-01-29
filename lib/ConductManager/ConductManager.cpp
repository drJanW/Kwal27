/**
 * @file ConductManager.cpp
 * @brief Central orchestrator for all Kwal modules
 * @version 251231E
 * @date 2025-12-31
 * 
 * The ConductManager is the main coordinator that:
 * - Initializes all modules via BootMaster at startup
 * - Routes intents from WebGUI to appropriate modules
 * - Manages lux measurement cycles (LED blackout for sensor reading)
 * - Handles audio fragment playback requests
 * - Coordinates OTA update window
 * 
 * Architecture follows the Boot→Plan→Policy→Conduct pattern:
 * - Boot: One-time initialization, hardware detection
 * - Plan: Timer scheduling for periodic tasks  
 * - Policy: Business logic and decision making
 * - Conduct: State management and execution
 * 
 * All timing uses TimerManager callbacks (no millis() or delay()).
 */

#include "LightManager.h"
#include "TimerManager.h"
#include "SensorManager.h"
#include "ConductManager.h"
#include "ContextManager.h"
#include "System/SystemBoot.h"
#include "Globals.h"
#include "PRTClock.h"
#include "BootMaster.h"
#include "AudioManager.h"
#include "AudioState.h"
#include "Status/StatusBoot.h"
#include "Status/StatusConduct.h"
#include "Status/StatusPolicy.h"
#include "Notify/NotifyConduct.h"
#include "Notify/NotifyState.h"
#include "WiFi/WiFiBoot.h"
#include "WiFi/WiFiConduct.h"
#include "WiFi/WiFiPolicy.h"
#include "Web/WebBoot.h"
#include "Web/WebConduct.h"
#include "Web/WebDirector.h"
#include "Web/WebPolicy.h"
#include "Audio/AudioBoot.h"
#include "Audio/AudioConduct.h"
#include "Audio/AudioPolicy.h"
#include "Audio/AudioDirector.h"
#include "Light/LightBoot.h"
#include "Light/LightConduct.h"
#include "Light/LightPolicy.h"
#include "Heartbeat/HeartbeatBoot.h"
#include "Heartbeat/HeartbeatConduct.h"
#include "Heartbeat/HeartbeatPolicy.h"
#include "Sensors/SensorsBoot.h"
#include "Sensors/SensorsConduct.h"
#include "Sensors/SensorsPolicy.h"
#include "OTA/OTABoot.h"
#include "OTA/OTAConduct.h"
#include "OTA/OTAPolicy.h"
#include "Speak/SpeakBoot.h"
#include "Speak/SpeakConduct.h"
#include "SD/SDBoot.h"
#include "SD/SDConduct.h"
#include "SD/SDPolicy.h"
#include "Calendar/CalendarBoot.h"
#include "Calendar/CalendarConduct.h"
#include "FetchManager.h"
#include "WiFiManager.h"
#include "Clock/ClockBoot.h"
#include "Clock/ClockConduct.h"
#include "Light/LightConduct.h"

// === Lux Measurement - delegated to LightConduct ===
void ConductManager::requestLuxMeasurement() {
    // Trigger manual lux measurement cycle via LightConduct
    LightConduct::cb_luxMeasure();
}

#ifndef LOG_CONDUCT_VERBOSE
#define LOG_CONDUCT_VERBOSE 0
#endif

#if LOG_CONDUCT_VERBOSE
#define CONDUCT_LOG_INFO(...)  LOG_INFO(__VA_ARGS__)
#define CONDUCT_LOG_DEBUG(...) LOG_DEBUG(__VA_ARGS__)
#else
#define CONDUCT_LOG_INFO(...)
#define CONDUCT_LOG_DEBUG(...)
#endif

#define CONDUCT_LOG_WARN(...)  LOG_WARN(__VA_ARGS__)
#define CONDUCT_LOG_ERROR(...) LOG_ERROR(__VA_ARGS__)

namespace {

void cb_clockUpdate() {
    prtClock.update();
}

void cb_sayTime() {
    // 75% INFORMAL, 25% split between FORMAL and NORMAL
    TimeStyle style = (random(0, 4) < 3) ? TimeStyle::INFORMAL : static_cast<TimeStyle>(random(0, 2));
    ConductManager::intentSayTime(style);
    // Schedule next with fresh random interval - unpredictable time announcements
    timers.restart(random(Globals::minSaytimeIntervalMs, Globals::maxSaytimeIntervalMs + 1), 1, cb_sayTime);
}

void cb_playFragment() {
    ConductManager::intentPlayFragment();
    // Schedule next with fresh random interval - the creature breathes
    timers.restart(random(Globals::minAudioIntervalMs, Globals::maxAudioIntervalMs + 1), 1, cb_playFragment);
}

void cb_bootFragment() {
    // Prerequisites guaranteed by stage 2 entry - no polling needed
    ConductManager::intentPlayFragment();
}

void cb_showTimerStatus() {
    ConductManager::intentShowTimerStatus();
}

static uint16_t webAudioNextFadeMs = 957U;  // local cache for callback

void cb_playNextFragment() {
    ConductManager::intentPlayFragment();
}

void cb_webAudioStopThenNext() {
    PlayAudioFragment::stop(webAudioNextFadeMs);
    timers.create(static_cast<uint32_t>(webAudioNextFadeMs) + 1U, 1, cb_playNextFragment);
}

} // namespace


static bool clockRunning = false;
static bool clockInFallback = false;

static StatusConduct statusConduct;
static ClockBoot clockBoot;
static ClockConduct clockConduct;
static SDBoot sdBoot;
static SDConduct sdConduct;
static WiFiBoot wifiBoot;
static WiFiConduct wifiConduct;
static WebBoot webBoot;
static WebConduct webConduct;
static AudioBoot audioBoot;
static AudioConduct audioConduct;
static LightBoot lightBoot;
static LightConduct lightConduct;
static SensorsBoot sensorsBoot;
static SensorsConduct sensorsConduct;
static OTABoot otaBoot;
static OTAConduct otaConduct;
static SpeakBoot speakBoot;
static SpeakConduct speakConduct;
static bool sdPostBootCompleted = false;

void ConductManager::begin() {
    // I2C already initialized in systemBootStage1()
    // First sayTime after random 45-145 min, then reschedules itself
    timers.create(random(Globals::minSaytimeIntervalMs, Globals::maxSaytimeIntervalMs + 1), 1, cb_sayTime);
    // First audio after random 6-18 min, then reschedules itself
    timers.create(random(Globals::minAudioIntervalMs, Globals::maxAudioIntervalMs + 1), 1, cb_playFragment);
    timers.create(Globals::timerStatusIntervalMs, 0, cb_showTimerStatus);
    timers.create(Globals::timeDisplayIntervalMs, 0, cb_timeDisplay);
    // Note: Periodic lux measurement is now handled by LightConduct::plan()
    bootMaster.begin();

    PL("[Stage 1] Core modules...");
    ContextManager::begin();
    PL("[Stage 1] Context manager started");
    heartbeatBoot.plan();
    heartbeatConduct.plan();
    statusBoot.plan();
    statusConduct.plan();
    clockBoot.plan();
    clockConduct.plan();

    PL("[Stage 1] SD probe...");
    if (!sdBoot.plan()) {
        return;
    }

    resumeAfterSDBoot();
}

void ConductManager::update() {
    audio.update();
#if LOG_HEARTBEAT
    static uint32_t lastHeartbeatMs = 0;
    uint32_t now = millis();
    if (now - lastHeartbeatMs >= 1000U) {
        LOG_HEARTBEAT_TICK('.');
        lastHeartbeatMs = now;
    }
#endif
}

void ConductManager::intentArmOTA(uint32_t window_s) {
    CONDUCT_LOG_INFO("[Conduct] intentArmOTA: window=%us\n", static_cast<unsigned>(window_s));
    otaArm(window_s);
    audio.stop();
    LightManager::instance().showOtaPattern();
}

bool ConductManager::intentConfirmOTA() {
    CONDUCT_LOG_INFO("[Conduct] intentConfirmOTA\n");
    return otaConfirmAndReboot();
}

void ConductManager::intentPlayFragment() {
    AudioFragment fragment{};
    if (!AudioDirector::selectRandomFragment(fragment)) {
        CONDUCT_LOG_WARN("[Conduct] intentPlayFragment: no fragment available\n");
        return;
    }

    if (!AudioPolicy::requestFragment(fragment)) {
        CONDUCT_LOG_WARN("[Conduct] intentPlayFragment: playback rejected\n");
    }
}

void ConductManager::intentPlaySpecificFragment(uint8_t dir, int8_t file) {
    FileEntry fileEntry{};
    uint8_t targetFile = static_cast<uint8_t>(file);
    
    // If file < 0, pick random file from dir
    if (file < 0) {
        DirEntry dirEntry{};
        if (!SDManager::readDirEntry(dir, &dirEntry) || dirEntry.fileCount == 0) {
            CONDUCT_LOG_WARN("[Conduct] intentPlaySpecificFragment: dir %u not found or empty\n", dir);
            return;
        }
        targetFile = random(0, dirEntry.fileCount);
    }
    
    if (!SDManager::readFileEntry(dir, targetFile, &fileEntry)) {
        CONDUCT_LOG_WARN("[Conduct] intentPlaySpecificFragment: file %u/%u not found\n", dir, targetFile);
        return;
    }
    
    const uint32_t rawDuration = static_cast<uint32_t>(fileEntry.sizeKb) * 1024UL / 24UL;  // BYTES_PER_MS approx
    if (rawDuration <= 200U) {
        CONDUCT_LOG_WARN("[Conduct] intentPlaySpecificFragment: file too short\n");
        return;
    }
    
    AudioFragment fragment{};
    fragment.dirIndex   = dir;
    fragment.fileIndex  = targetFile;
    fragment.score      = fileEntry.score;
    fragment.startMs    = 100U;  // Skip header
    fragment.durationMs = rawDuration - 100U;
    fragment.fadeMs     = 500U;  // Default fade
    
    if (!AudioPolicy::requestFragment(fragment)) {
        CONDUCT_LOG_WARN("[Conduct] intentPlaySpecificFragment: playback rejected\n");
    }
}

// Static flag to ensure boot fragment only plays once
static bool bootFragmentTriggered = false;

void ConductManager::triggerBootFragment() {
    if (bootFragmentTriggered) {
        return;  // Only once
    }
    bootFragmentTriggered = true;
    timers.create(500, 1, cb_bootFragment);
}

void ConductManager::intentSayTime(TimeStyle style) {
    const String sentence = prtClock.buildTimeSentence(style);
    if (sentence.isEmpty()) {
        CONDUCT_LOG_WARN("[Conduct] intentSayTime: clock sentence empty\n");
        return;
    }
    AudioPolicy::requestSentence(sentence);
}

void ConductManager::intentSetAudioLevel(float value) {
    // F9 pattern: webShift can be >1.0, no clamp
    audio.setVolumeWebShift(value);
    CONDUCT_LOG_INFO("[Conduct] intentSetAudioLevel: webShift=%.2f\n",
                     static_cast<double>(value));
}

void ConductManager::intentShowTimerStatus() {
    timers.showAvailableTimers(true);
}

bool ConductManager::intentStartClockTick(bool fallbackMode) {
    if (clockRunning && clockInFallback == fallbackMode) {
        return true;
    }

    bool wasRunning = clockRunning;
    if (!timers.create(SECONDS_TICK, 0, cb_clockUpdate)) {
        CONDUCT_LOG_ERROR("[Conduct] Failed to start clock tick (%s)\n", fallbackMode ? "fallback" : "normal");
        if (wasRunning) {
            clockRunning = false;
        }
        return false;
    }

    clockRunning = true;
    clockInFallback = fallbackMode;
    CONDUCT_LOG_INFO("[Conduct] Clock tick running (%s)\n", fallbackMode ? "fallback" : "normal");
    return true;
}

bool ConductManager::isClockRunning() {
    return clockRunning;
}

bool ConductManager::isClockInFallback() {
    return clockInFallback;
}

bool ConductManager::intentSeedClockFromRtc() {
    return clockConduct.seedClockFromRtc(prtClock);
}

void ConductManager::intentSyncRtcFromClock() {
    clockConduct.syncRtcFromClock(prtClock);
}

void ConductManager::resumeAfterSDBoot() {
    if (sdPostBootCompleted) {
        return;
    }

    sdPostBootCompleted = true;

    PL("[Stage 1] Post-SD modules...");
    sdConduct.plan();
    calendarBoot.plan();
    calendarConduct.plan();
    wifiBoot.plan();
    wifiConduct.plan();
    webBoot.plan();
    webConduct.plan();
    WebDirector::instance().plan();
    lightBoot.plan();
    lightConduct.plan();
    audioBoot.plan();
    audioConduct.plan();
    sensorsBoot.plan();
    sensorsConduct.plan();
    otaBoot.plan();
    otaConduct.plan();
    speakBoot.plan();
    speakConduct.plan();
    PL("[Stage 1] Complete - Stage 2 actions via OK reports");
    // Stage 2 triggered per-component when OK reported (WIFI_OK, AUDIO_OK, etc.)
}

void ConductManager::intentWebAudioNext(uint16_t fadeMs) {
    webAudioNextFadeMs = fadeMs;
    timers.cancel(cb_webAudioStopThenNext);
    timers.create(1, 1, cb_webAudioStopThenNext);
}
