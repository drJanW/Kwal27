/**
 * @file RunManager.cpp
 * @brief Central run coordinator for all Kwal modules
 * @version 260204A
 * @date 2026-02-04
 */
#include <Arduino.h>
#include <math.h>
#include "LightController.h"
#include "TimerManager.h"
#include "SensorController.h"
#include "RunManager.h"
#include "ContextController.h"
#include "System/SystemBoot.h"
#include "Globals.h"
#include "PRTClock.h"
#include "BootManager.h"
#include "AudioManager.h"
#include "AudioState.h"
#include "Status/StatusBoot.h"
#include "Status/StatusRun.h"
#include "Status/StatusPolicy.h"
#include "Alert/AlertRun.h"
#include "Alert/AlertState.h"
#include "WiFi/WiFiBoot.h"
#include "WiFi/WiFiRun.h"
#include "WiFi/WiFiPolicy.h"
#include "Web/WebBoot.h"
#include "Web/WebRun.h"
#include "Web/WebDirector.h"
#include "Web/WebPolicy.h"
#include "Audio/AudioBoot.h"
#include "Audio/AudioRun.h"
#include "Audio/AudioPolicy.h"
#include "Audio/AudioDirector.h"
#include "Light/LightBoot.h"
#include "Light/LightRun.h"
#include "Light/LightPolicy.h"
#include "Heartbeat/HeartbeatBoot.h"
#include "Heartbeat/HeartbeatRun.h"
#include "Heartbeat/HeartbeatPolicy.h"
#include "Sensors/SensorsBoot.h"
#include "Sensors/SensorsRun.h"
#include "Sensors/SensorsPolicy.h"
#include "OTA/OTABoot.h"
#include "OTA/OTARun.h"
#include "OTA/OTAPolicy.h"
#include "Speak/SpeakBoot.h"
#include "Speak/SpeakRun.h"
#include "SD/SDBoot.h"
#include "SD/SDRun.h"
#include "SD/SDPolicy.h"
#include "Calendar/CalendarBoot.h"
#include "Calendar/CalendarRun.h"
#include "FetchController.h"
#include "WiFiController.h"
#include "Clock/ClockBoot.h"
#include "Clock/ClockRun.h"
#include "Light/LightRun.h"

// === Lux Measurement - delegated to LightRun ===
void RunManager::requestLuxMeasurement() {
    // Trigger manual lux measurement cycle via LightRun
    LightRun::cb_luxMeasure();
}

#ifndef LOG_RUN_VERBOSE
#define LOG_RUN_VERBOSE 0
#endif

#if LOG_RUN_VERBOSE
#define RUN_LOG_INFO(...)  LOG_INFO(__VA_ARGS__)
#define RUN_LOG_DEBUG(...) LOG_DEBUG(__VA_ARGS__)
#else
#define RUN_LOG_INFO(...)
#define RUN_LOG_DEBUG(...)
#endif

#define RUN_LOG_WARN(...)  LOG_WARN(__VA_ARGS__)
#define RUN_LOG_ERROR(...) LOG_ERROR(__VA_ARGS__)

namespace {

void cb_clockUpdate() {
    prtClock.update();
}

void cb_sayTime() {
    // 75% INFORMAL, 25% split between FORMAL and NORMAL
    TimeStyle style = (random(0, 4) < 3) ? TimeStyle::INFORMAL : static_cast<TimeStyle>(random(0, 2));
    RunManager::requestSayTime(style);
    // Schedule next with fresh random interval - unpredictable time announcements
    timers.restart(random(Globals::minSaytimeIntervalMs, Globals::maxSaytimeIntervalMs + 1), 1, cb_sayTime);
}

String buildTemperatureSentence(float tempC) {
    char tempBuf[16];
    const float roundedOneDecimal = roundf(tempC * 10.0f) / 10.0f;
    const float roundedWhole = roundf(roundedOneDecimal);
    if (fabsf(roundedOneDecimal - roundedWhole) < 0.01f) {
        snprintf(tempBuf, sizeof(tempBuf), "%d", static_cast<int>(roundedWhole));
    } else {
        snprintf(tempBuf, sizeof(tempBuf), "%.1f", static_cast<double>(roundedOneDecimal));
    }
    for (char* p = tempBuf; *p != '\0'; ++p) {
        if (*p == '.') {
            *p = ',';
        }
    }
    String sentence = "het is ";
    sentence += tempBuf;
    sentence += " graden celsius";
    return sentence;
}

void cb_sayTemperature() {
    RUN_LOG_INFO("[Run] cb_sayTemperature fired\n");
    RunManager::requestSayTemperature();
    timers.restart(random(Globals::minTemperatureSpeakIntervalMs,
                          Globals::maxTemperatureSpeakIntervalMs + 1),
                   1, cb_sayTemperature);
}

void cb_playFragment() {
    RunManager::requestPlayFragment();
    // Schedule next with fresh random interval - the creature breathes
    timers.restart(random(Globals::minAudioIntervalMs, Globals::maxAudioIntervalMs + 1), 1, cb_playFragment);
}

void cb_bootFragment() {
    // Prerequisites guaranteed by stage 2 entry - no polling needed
    RunManager::requestPlayFragment();
}

void cb_showTimerStatus() {
    RunManager::requestShowTimerStatus();
}

static uint16_t webAudioNextFadeMs = 957U;  // local cache for callback

void cb_playNextFragment() {
    RunManager::requestPlayFragment();
}

void cb_webAudioStopThenNext() {
    PlayAudioFragment::stop(webAudioNextFadeMs);
    timers.create(static_cast<uint32_t>(webAudioNextFadeMs) + 1U, 1, cb_playNextFragment);
}

} // namespace


static bool clockRunning = false;
static bool clockInFallback = false;

static StatusRun statusRun;
static ClockBoot clockBoot;
static ClockRun clockRun;
static SDBoot sdBoot;
static SDRun sdRun;
static WiFiBoot wifiBoot;
static WiFiRun wifiRun;
static WebBoot webBoot;
static WebRun webRun;
static AudioBoot audioBoot;
static AudioRun audioRun;
static LightBoot lightBoot;
static LightRun lightRun;
static SensorsBoot sensorsBoot;
static SensorsRun sensorsRun;
static OTABoot otaBoot;
static OTARun otaRun;
static SpeakBoot speakBoot;
static SpeakRun speakRun;
static bool sdPostBootCompleted = false;
static bool wifiPostBootCompleted = false;

void RunManager::begin() {
    // I2C already initialized in systemBootStage1()
    // First sayTime after random 45-145 min, then reschedules itself
    timers.create(random(Globals::minSaytimeIntervalMs, Globals::maxSaytimeIntervalMs + 1), 1, cb_sayTime);
    timers.create(random(Globals::minTemperatureSpeakIntervalMs, Globals::maxTemperatureSpeakIntervalMs + 1),
                  1, cb_sayTemperature);
    // First audio after random 6-18 min, then reschedules itself
    timers.create(random(Globals::minAudioIntervalMs, Globals::maxAudioIntervalMs + 1), 1, cb_playFragment);
    timers.create(Globals::timerStatusIntervalMs, 0, cb_showTimerStatus);
    timers.create(Globals::timeDisplayIntervalMs, 0, cb_timeDisplay);
    // Note: Periodic lux measurement is now handled by LightRun::plan()
    bootManager.begin();

    PL("[Stage 1] Core modules...");
    ContextController::begin();
    PL("[Stage 1] Context controller started");
    heartbeatBoot.plan();
    heartbeatRun.plan();
    statusBoot.plan();
    statusRun.plan();
    clockBoot.plan();
    clockRun.plan();

    PL("[Stage 1] SD probe...");
    if (!sdBoot.plan()) {
        return;
    }

    resumeAfterSDBoot();
}

void RunManager::update() {
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

void RunManager::requestArmOTA(uint32_t window_s) {
    RUN_LOG_INFO("[Run] requestArmOTA: window=%us\n", static_cast<unsigned>(window_s));
    otaArm(window_s);
    audio.stop();
    lightController.showOtaPattern();
}

bool RunManager::requestConfirmOTA() {
    RUN_LOG_INFO("[Run] requestConfirmOTA\n");
    return otaConfirmAndReboot();
}

void RunManager::requestPlayFragment() {
    AudioFragment fragment{};
    if (!AudioDirector::selectRandomFragment(fragment)) {
        RUN_LOG_WARN("[Run] requestPlayFragment: no fragment available\n");
        return;
    }

    if (!AudioPolicy::requestFragment(fragment)) {
        RUN_LOG_WARN("[Run] requestPlayFragment: playback rejected\n");
    }
}

void RunManager::requestPlaySpecificFragment(uint8_t dir, int8_t file) {
    FileEntry fileEntry{};
    uint8_t targetFile = static_cast<uint8_t>(file);
    
    // If file < 0, pick random file from dir
    if (file < 0) {
        DirEntry dirEntry{};
        if (!SDController::readDirEntry(dir, &dirEntry) || dirEntry.fileCount == 0) {
            RUN_LOG_WARN("[Run] requestPlaySpecificFragment: dir %u not found or empty\n", dir);
            return;
        }
        targetFile = random(0, dirEntry.fileCount);
    }
    
    if (!SDController::readFileEntry(dir, targetFile, &fileEntry)) {
        RUN_LOG_WARN("[Run] requestPlaySpecificFragment: file %u/%u not found\n", dir, targetFile);
        return;
    }
    
    const uint32_t rawDuration = static_cast<uint32_t>(fileEntry.sizeKb) * 1024UL / 24UL;  // BYTES_PER_MS approx
    if (rawDuration <= 200U) {
        RUN_LOG_WARN("[Run] requestPlaySpecificFragment: file too short\n");
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
        RUN_LOG_WARN("[Run] requestPlaySpecificFragment: playback rejected\n");
    }
}

// Static flag to ensure boot fragment only plays once
static bool bootFragmentTriggered = false;

void RunManager::triggerBootFragment() {
    if (bootFragmentTriggered) {
        return;  // Only once
    }
    bootFragmentTriggered = true;
    timers.create(500, 1, cb_bootFragment);
}

void RunManager::requestSayTime(TimeStyle style) {
    const String sentence = prtClock.buildTimeSentence(style);
    if (sentence.isEmpty()) {
        RUN_LOG_WARN("[Run] requestSayTime: clock sentence empty\n");
        return;
    }
    AudioPolicy::requestSentence(sentence);
}

void RunManager::requestSayTemperature() {
    const auto& ctx = ContextController::time();
    const float tempC = ctx.rtcTemperatureC;
    RUN_LOG_INFO("[Run] requestSayTemperature: temp=%.2f\n", static_cast<double>(tempC));
    const String sentence = buildTemperatureSentence(tempC);
    if (sentence.isEmpty()) {
        return;
    }
    AudioPolicy::requestSentence(sentence);
}

void RunManager::requestSetAudioLevel(float value) {
    // F9 pattern: webShift can be >1.0, no clamp
    audio.setVolumeWebShift(value);
    RUN_LOG_INFO("[Run] requestSetAudioLevel: webShift=%.2f\n",
                     static_cast<double>(value));
}

void RunManager::requestShowTimerStatus() {
    timers.showAvailableTimers(true);
}

bool RunManager::requestStartClockTick(bool fallbackEnabled) {
    if (clockRunning && clockInFallback == fallbackEnabled) {
        return true;
    }

    bool wasRunning = clockRunning;
    if (!timers.create(SECONDS_TICK, 0, cb_clockUpdate)) {
        RUN_LOG_ERROR("[Run] Failed to start clock tick (%s)\n", fallbackEnabled ? "fallback" : "normal");
        if (wasRunning) {
            clockRunning = false;
        }
        return false;
    }

    clockRunning = true;
    clockInFallback = fallbackEnabled;
    RUN_LOG_INFO("[Run] Clock tick running (%s)\n", fallbackEnabled ? "fallback" : "normal");
    return true;
}

bool RunManager::isClockRunning() {
    return clockRunning;
}

bool RunManager::isClockInFallback() {
    return clockInFallback;
}

bool RunManager::requestSeedClockFromRtc() {
    return clockRun.seedClockFromRtc(prtClock);
}

void RunManager::requestSyncRtcFromClock() {
    clockRun.syncRtcFromClock(prtClock);
}

void RunManager::resumeAfterSDBoot() {
    if (sdPostBootCompleted) {
        return;
    }

    sdPostBootCompleted = true;

    PL("[Stage 1] Post-SD modules...");
    sdRun.plan();
    wifiBoot.plan();
    wifiRun.plan();
    webBoot.plan();
    webRun.plan();
    WebDirector::instance().plan();
    sensorsBoot.plan();
    sensorsRun.plan();
    otaBoot.plan();
    otaRun.plan();
    speakBoot.plan();
    speakRun.plan();
    PL("[Stage 1] Waiting for CSV fetch");
    // Stage 2 triggered per-component when OK reported (WIFI_OK, AUDIO_OK, etc.)
}

void RunManager::resumeAfterWiFiBoot() {
    if (wifiPostBootCompleted) {
        return;
    }

    wifiPostBootCompleted = true;

    PL("[Stage 1] CSV modules...");
    Globals::begin();
    bootManager.restartBootTimer();
    calendarBoot.plan();
    calendarRun.plan();
    lightBoot.plan();
    lightRun.plan();
    audioBoot.plan();
    audioRun.plan();
    PL("[Stage 1] Complete - Stage 2 actions via OK reports");
}

void RunManager::requestWebAudioNext(uint16_t fadeMs) {
    webAudioNextFadeMs = fadeMs;
    timers.cancel(cb_webAudioStopThenNext);
    timers.create(1, 1, cb_webAudioStopThenNext);
}
