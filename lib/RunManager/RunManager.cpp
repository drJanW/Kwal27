/**
 * @file RunManager.cpp
 * @brief Central run coordinator for all Kwal modules
 * @version 260227A
 * @date 2026-02-26
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
#include "PlaySentence.h"
#include "PlayFragment.h"
#include "WebGuiStatus.h"
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

// ─── Daily auto-reboot ──────────────────────────────────────

uint8_t rebootRetries = 0;
constexpr uint8_t kMaxRebootRetries = 30;

void cb_dailyReboot() {
    // Guard: don't reboot mid-write or mid-speech
    if (AlertState::isSdBusy() || isSentencePlaying() || isFragmentPlaying()) {
        if (++rebootRetries <= kMaxRebootRetries) {
            PF("[Reboot] busy, retry %u/%u in 1 min\n", rebootRetries, kMaxRebootRetries);
            timers.restart(MINUTES(1), 1, cb_dailyReboot);
        } else {
            PL("[Reboot] still busy after 30 min — rebooting anyway");
            Serial.flush();
            ESP.restart();
        }
        return;
    }
    PL("[Reboot] Daily scheduled reboot");
    Serial.flush();
    ESP.restart();
}

// Calculate ms from now until target hour (next occurrence)
uint32_t calcMsUntilHour(uint8_t targetHour) {
    const int16_t nowMin = static_cast<int16_t>(prtClock.getHour()) * 60
                         + prtClock.getMinute();
    const int16_t targetMin = static_cast<int16_t>(targetHour) * 60;
    int16_t deltaMin = targetMin - nowMin;
    if (deltaMin <= 5) deltaMin += 1440;  // Next day (skip if <5 min away)
    return static_cast<uint32_t>(deltaMin) * 60000UL;
}

void armDailyReboot() {
    if (Globals::dailyRebootHour == 0) return;  // Disabled
    if (timers.isActive(cb_dailyReboot)) return; // Already armed
    if (!prtClock.isTimeFetched()) return;        // No valid time yet

    rebootRetries = 0;
    const uint32_t delayMs = calcMsUntilHour(Globals::dailyRebootHour);
    timers.create(delayMs, 1, cb_dailyReboot);
    const uint16_t totalMin = static_cast<uint16_t>(delayMs / 60000UL);
    PF("[Reboot] Armed at %02u:00, in %uu%02u\n",
       Globals::dailyRebootHour, totalMin / 60, totalMin % 60);
}

// ─── Clock tick ─────────────────────────────────────────────

void cb_clockUpdate() {
    static uint8_t lastDay = 0;
    prtClock.update();

    // Arm daily reboot once clock is valid (idempotent)
    armDailyReboot();

    // Detect day change → reload calendar for new day
    const uint8_t currentDay = prtClock.getDay();
    if (lastDay != 0 && currentDay != lastDay) {
        PF("[ClockRun] Day changed %u → %u, reloading calendar\n", lastDay, currentDay);
        timers.restart(SECONDS(5), 1, CalendarRun::cb_loadCalendar);
    }
    lastDay = currentDay;
}

void cb_sayTime() {
    // 75% INFORMAL, 25% split between FORMAL and NORMAL
    TimeStyle style = (random(0, 4) < 3) ? TimeStyle::INFORMAL : static_cast<TimeStyle>(random(0, 2));
    RunManager::requestSayTime(style);
    // Schedule next with fresh random interval - unpredictable time announcements
    timers.restart(random(AudioPolicy::effectiveSpeakMin(), AudioPolicy::effectiveSpeakMax() + 1), 1, cb_sayTime);
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

void cb_sayRTCtemperature() {
    RUN_LOG_INFO("[ClockRun] cb_sayRTCtemperature\n");
    RunManager::requestSayRTCtemperature();
    timers.restart(random(AudioPolicy::effectiveSpeakMin(),
                          AudioPolicy::effectiveSpeakMax() + 1),
                   1, cb_sayRTCtemperature);
}

void cb_playFragment() {
    RunManager::requestPlayFragment();
    // Schedule next: explicit web range wins, then web-singleDir, then Globals
    uint32_t lo = AudioPolicy::effectiveFragmentMin();
    uint32_t hi = AudioPolicy::effectiveFragmentMax();
    if (!AudioPolicy::isWebFragmentRangeActive()
        && AudioPolicy::themeBoxId().startsWith("web-")) {
        lo = Globals::singleDirMinIntervalMs;
        hi = Globals::singleDirMaxIntervalMs;
    }
    timers.restart(random(lo, hi + 1), 1, cb_playFragment);
}

void cb_bootFragment() {
    // Wait for audio to be idle (TTS or previous fragment)
    if (isSentencePlaying() || isFragmentPlaying()) {
        return;  // Timer will fire again
    }
    timers.cancel(cb_bootFragment);  // Success — stop retrying
    RunManager::requestPlayFragment();
}



static uint16_t webAudioNextFadeMs = 957U;  // local cache for callback
static AudioFragment pendingFragment{};     // stashed fragment for stop-then-play
static bool hasPendingFragment = false;

void cb_playNextFragment() {
    RunManager::requestPlayFragment("random");
}

void cb_webAudioStopThenNext() {
    PlayAudioFragment::stop(webAudioNextFadeMs);
    timers.create(static_cast<uint32_t>(webAudioNextFadeMs) + 1U, 1, cb_playNextFragment);
}

void cb_playPendingFragment() {
    if (!hasPendingFragment) return;
    hasPendingFragment = false;
    if (!AudioPolicy::requestFragment(pendingFragment)) {
        RUN_LOG_WARN("[AudioRun] playback rejected\n");
    }
}

void cb_stopThenPlayPending() {
    constexpr uint16_t kInterruptFadeMs = 500U;
    PlayAudioFragment::stop(kInterruptFadeMs);
    timers.create(kInterruptFadeMs + 1U, 1, cb_playPendingFragment);
}

void cb_startSync() {
    PlayAudioFragment::stop(0);  // Immediate stop — no fade during sync
    AlertState::setSyncMode(true);
}

// ─── Web audio interval/silence support ─────────────────────

uint32_t webExpiryMs = Globals::defaultWebExpiryMs;

struct PendingAudioIntervals {
    uint32_t speakMinMs   = 0;
    uint32_t speakMaxMs   = 0;
    uint32_t fragMinMs    = 0;
    uint32_t fragMaxMs    = 0;
    uint32_t durationMs   = 0;
    bool     silence      = false;
    bool     hasSpeakRange = false;
    bool     hasFragRange  = false;
} pendingIntervals;

void cb_clearWebAudio();  // forward declare for cb_applyAudioIntervals

void cb_applyAudioIntervals() {
    auto& p = pendingIntervals;

    PF("[WebAudio] speak=%s frag=%s dur=%um\n",
       p.hasSpeakRange ? String((p.speakMinMs + p.speakMaxMs) / 2U / 60000U).c_str() : "-",
       p.hasFragRange  ? String((p.fragMinMs  + p.fragMaxMs)  / 2U / 60000U).c_str() : "-",
       static_cast<unsigned>(p.durationMs / 60000U));

    if (p.hasSpeakRange)
        AudioPolicy::setWebSpeakRange(p.speakMinMs, p.speakMaxMs);
    if (p.hasFragRange)
        AudioPolicy::setWebFragmentRange(p.fragMinMs, p.fragMaxMs);
    AudioPolicy::setWebSilence(p.silence);

    webExpiryMs = p.durationMs;

    // Arm expiry timer — external arming → cancel + create
    timers.cancel(cb_clearWebAudio);
    timers.create(p.durationMs, 1, cb_clearWebAudio);

    if (p.silence) {
        PlayAudioFragment::stop(0);
        PlaySentence::stop();
    }

    // Reschedule speak/fragment timers with new ranges
    timers.cancel(cb_sayTime);
    timers.create(
        random(AudioPolicy::effectiveSpeakMin(),
               AudioPolicy::effectiveSpeakMax() + 1),
        1, cb_sayTime);
    timers.cancel(cb_playFragment);
    timers.create(
        random(AudioPolicy::effectiveFragmentMin(),
               AudioPolicy::effectiveFragmentMax() + 1),
        1, cb_playFragment);
}

void cb_clearWebAudio() {
    AudioPolicy::clearWebSpeakRange();
    AudioPolicy::clearWebFragmentRange();
    AudioPolicy::setWebSilence(false);
    audio.setVolumeWebMultiplier(1.0f);
    webExpiryMs = Globals::defaultWebExpiryMs;

    // Reschedule with Globals defaults
    timers.cancel(cb_sayTime);
    timers.create(
        random(Globals::minSaytimeIntervalMs,
               Globals::maxSaytimeIntervalMs + 1),
        1, cb_sayTime);
    timers.cancel(cb_playFragment);
    timers.create(
        random(Globals::minAudioIntervalMs,
               Globals::maxAudioIntervalMs + 1),
        1, cb_playFragment);

    // Trigger SSE push so WebGUI sliders snap back to defaults
    WebGuiStatus::pushState();
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
static SpeakBoot speakBoot;
static SpeakRun speakRun;
static bool sdPostBootCompleted = false;
static bool wifiPostBootCompleted = false;

void RunManager::begin() {
    // I2C already initialized in systemBootStage1()
    // First sayTime after random 45-145 min, then reschedules itself
    timers.create(random(Globals::minSaytimeIntervalMs, Globals::maxSaytimeIntervalMs + 1), 1, cb_sayTime);
    timers.create(random(Globals::minTemperatureSpeakIntervalMs, Globals::maxTemperatureSpeakIntervalMs + 1),
                  1, cb_sayRTCtemperature);
    // First audio after random 6-18 min, then reschedules itself
    timers.create(random(Globals::minAudioIntervalMs, Globals::maxAudioIntervalMs + 1), 1, cb_playFragment);

    // Note: Periodic lux measurement is now handled by LightRun::plan()
    bootManager.begin();

    ContextController::begin();
    heartbeatBoot.plan();
    heartbeatRun.plan();
    statusBoot.plan();
    statusRun.plan();
    clockBoot.plan();
    clockRun.plan();

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

void RunManager::requestPlayFragment(const char* source) {
    if (!AlertState::canPlayFragment()) {
        RUN_LOG_WARN("[AudioRun] playback blocked by policy\n");
        return;
    }
    AudioFragment fragment{};
    if (!AudioDirector::selectRandomFragment(fragment)) {
        RUN_LOG_WARN("[AudioRun] no fragment available\n");
        return;
    }
    // Source tag only — no box info
    strncpy(fragment.source, source, sizeof(fragment.source) - 1);
    fragment.source[sizeof(fragment.source) - 1] = '\0';

    if (!AudioPolicy::requestFragment(fragment)) {
        RUN_LOG_WARN("[AudioRun] playback rejected\n");
    }
}

void RunManager::requestPlaySpecificFragment(uint8_t dir, int8_t file, const char* source) {
    if (!AlertState::canPlayFragment()) {
        RUN_LOG_WARN("[AudioRun] playback blocked by policy\n");
        return;
    }
    AudioPolicy::resetToBaseThemeBox();  // Clear any single-dir override
    FileEntry fileEntry{};
    uint8_t targetFile = static_cast<uint8_t>(file);
    
    // If file < 0, pick random file from dir
    if (file < 0) {
        DirEntry dirEntry{};
        if (!SDController::readDirEntry(dir, &dirEntry) || dirEntry.fileCount == 0) {
            RUN_LOG_WARN("[AudioRun] dir %u not found or empty\n", dir);
            return;
        }
        targetFile = random(1, dirEntry.fileCount + 1);
    }
    
    if (!SDController::readFileEntry(dir, targetFile, &fileEntry)) {
        RUN_LOG_WARN("[AudioRun] file %u/%u not found\n", dir, targetFile);
        return;
    }
    
    const uint32_t rawDuration = static_cast<uint32_t>(fileEntry.sizeKb) * 1024UL / 24UL;  // BYTES_PER_MS approx
    if (rawDuration <= 200U) {
        RUN_LOG_WARN("[AudioRun] file too short\n");
        return;
    }
    
    AudioFragment fragment{};
    fragment.dirIndex   = dir;
    fragment.fileIndex  = targetFile;
    fragment.score      = fileEntry.score;
    fragment.startMs    = 100U;  // Skip header
    fragment.durationMs = rawDuration - 100U;
    fragment.fadeMs     = 500U;  // Default fade
    strncpy(fragment.source, source, sizeof(fragment.source) - 1);
    fragment.source[sizeof(fragment.source) - 1] = '\0';
    
    if (isAudioBusy()) {
        // Stash fragment, stop current, play after fade-out
        pendingFragment = fragment;
        hasPendingFragment = true;
        timers.cancel(cb_stopThenPlayPending);
        timers.create(1, 1, cb_stopThenPlayPending);
        return;
    }
    
    if (!AudioPolicy::requestFragment(fragment)) {
        RUN_LOG_WARN("[AudioRun] playback rejected\n");
    }
}

void RunManager::requestSetSingleDirThemeBox(uint8_t dir) {
    AudioPolicy::setThemeBox(&dir, 1, "web-" + String(dir));
    requestPlaySpecificFragment(dir, -1, "grid/dir");  // Play random file from that dir immediately
    // Reschedule next automatic play with shorter interval
    timers.restart(random(Globals::singleDirMinIntervalMs, Globals::singleDirMaxIntervalMs + 1), 1, cb_playFragment);
}

// Static flag to ensure boot fragment only plays once
static bool bootFragmentTriggered = false;

void RunManager::triggerBootFragment() {
    if (bootFragmentTriggered) {
        return;  // Only once
    }
    bootFragmentTriggered = true;
    timers.create(500, 30, cb_bootFragment);  // Poll until audio idle, self-cancels
}

void RunManager::requestSayTime(TimeStyle style) {
    const String sentence = prtClock.buildTimeSentence(style);
    if (sentence.isEmpty()) {
        RUN_LOG_WARN("[ClockRun] sentence empty\n");
        return;
    }
    AudioPolicy::requestSentence(sentence);
}

void RunManager::requestSayRTCtemperature() {
    const auto& ctx = ContextController::time();
    if (!ctx.hasRtcTemperature) return;
    const float tempC = ctx.rtcTemperatureC;
    if (tempC < 75.0f) return;  // Only speak when overheating
    RUN_LOG_INFO("[ClockRun] sayRTCtemperature temp=%.1f\n", static_cast<double>(tempC));
    const String sentence = buildTemperatureSentence(tempC);
    if (sentence.isEmpty()) {
        return;
    }
    AudioPolicy::requestSentence(sentence);
}

void RunManager::requestSetAudioLevel(float value) {
    // F9 pattern: webMultiplier can be >1.0, no clamp
    audio.setVolumeWebMultiplier(value);
    // Arm/reset shared expiry — any web audio change resets countdown
    timers.cancel(cb_clearWebAudio);
    timers.create(webExpiryMs, 1, cb_clearWebAudio);
    RUN_LOG_INFO("[AudioRun] webMultiplier=%.2f\n",
                     static_cast<double>(value));
}

void RunManager::requestSetAudioIntervals(
    uint32_t speakMinMs, uint32_t speakMaxMs, bool hasSpeakRange,
    uint32_t fragMinMs,  uint32_t fragMaxMs,  bool hasFragRange,
    bool silence, uint32_t durationMs)
{
    pendingIntervals = {speakMinMs, speakMaxMs, fragMinMs, fragMaxMs,
                        durationMs, silence, hasSpeakRange, hasFragRange};
    timers.cancel(cb_applyAudioIntervals);
    timers.create(1, 1, cb_applyAudioIntervals);
}

void RunManager::requestSetSilence(bool active) {
    AudioPolicy::setWebSilence(active);
    if (active) {
        PlayAudioFragment::stop(0);
        PlaySentence::stop();
    }
    // Arm/reset shared expiry
    timers.cancel(cb_clearWebAudio);
    timers.create(webExpiryMs, 1, cb_clearWebAudio);
}

bool RunManager::requestStartClockTick(bool fallbackEnabled) {
    if (clockRunning && clockInFallback == fallbackEnabled) {
        return true;
    }

    bool wasRunning = clockRunning;
    if (!timers.create(SECONDS_TICK, 0, cb_clockUpdate)) {
        RUN_LOG_ERROR("[ClockRun] Failed to start tick (%s)\n", fallbackEnabled ? "fallback" : "normal");
        if (wasRunning) {
            clockRunning = false;
        }
        return false;
    }

    clockRunning = true;
    clockInFallback = fallbackEnabled;
    RUN_LOG_INFO("[ClockRun] tick started (%s)\n", fallbackEnabled ? "fallback" : "normal");
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

    // When SD failed, load NVS WiFi cache BEFORE WiFi connects
    // (normal SD path already loaded config.txt via Globals::begin() in SDBoot::initSD)
    if (!AlertState::isSdOk()) {
        Globals::begin();
        PF("\n=== DEGRADED MODE (no SD) ===\n");
        PF("  Device:  %s\n", Globals::deviceName);
        PF("  IP:      %s\n", strlen(Globals::staticIp) > 0 ? Globals::staticIp : "DHCP");
        PF("  Active:  LED fallback, TTS, WebGUI fallback, OTA\n");
        PF("  Missing: music, animated light shows (and calendar, config)\n");
        PF("  Action:  insert SD card and restart\n");
        PF("=============================\n\n");
    }

    sdRun.plan();
    wifiBoot.plan();
    wifiRun.plan();
    webBoot.plan();
    webRun.plan();
    WebDirector::instance().plan();
    sensorsBoot.plan();
    sensorsRun.plan();
    speakBoot.plan();
    speakRun.plan();
}

void RunManager::resumeAfterWiFiBoot() {
    if (wifiPostBootCompleted) {
        return;
    }

    wifiPostBootCompleted = true;

    // Globals::begin() already called during SD boot (or SD-fail fallback)
    bootManager.restartBootTimer();
    calendarBoot.plan();
    calendarRun.plan();
    lightBoot.plan();
    lightRun.plan();
    audioBoot.plan();
    audioRun.plan();
}

void RunManager::requestWebAudioNext(uint16_t fadeMs) {
    AudioPolicy::resetToBaseThemeBox();  // Clear any single-dir override
    webAudioNextFadeMs = fadeMs;
    timers.cancel(cb_webAudioStopThenNext);
    timers.create(1, 1, cb_webAudioStopThenNext);
}

void RunManager::requestStartSync() {
    timers.cancel(cb_startSync);
    timers.create(1, 1, cb_startSync);
}

void RunManager::requestStopSync() {
    AlertState::setSyncMode(false);
}
