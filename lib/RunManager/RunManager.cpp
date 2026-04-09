/**
 * @file RunManager.cpp
 * @brief Central run coordinator for all Kwal modules
 * @version 260409A
 * @date 2026-04-09
 */
#include <Arduino.h>
#include <math.h>
#include <esp_sleep.h>
#include <SD.h>
#include <WiFi.h>
#include "LightController.h"
#include "TvShow.h"
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
#include "Alert/AlertRun.h"
#include "Alert/AlertState.h"
#include "TodayState.h"
#include "WiFi/WiFiBoot.h"
#include "WiFi/WiFiRun.h"
#include "Web/WebBoot.h"
#include "Web/WebRun.h"
#include "Web/WebDirector.h"
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
#include "Boot/BootSequencer.h"
#include "Boot/Cap.h"

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
constexpr uint8_t maxRebootRetries = 30;

void cb_dailyReboot() {
    // Guard: don't reboot mid-write or mid-speech
    if (AlertState::isSdBusy() || isSentencePlaying() || isFragmentPlaying()) {
        if (++rebootRetries <= maxRebootRetries) {
            PF("[Reboot] busy, retry %u/%u in 1 min\n", rebootRetries, maxRebootRetries);
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

// ─── Deep sleep ─────────────────────────────────────────────

uint8_t sleepRetries = 0;
constexpr uint8_t maxSleepRetries = 30;
constexpr uint32_t sleepCooldownMs = 60000;  // Refuse sleep within 60s of boot

// Calculate ms from now until target hour:minute (next occurrence)
uint32_t calcMsUntilTime(uint8_t targetHour, uint8_t targetMinute) {
    const int16_t nowMin = static_cast<int16_t>(prtClock.getHour()) * 60
                         + prtClock.getMinute();
    const int16_t targetMin = static_cast<int16_t>(targetHour) * 60
                            + targetMinute;
    int16_t deltaMin = targetMin - nowMin;
    if (deltaMin <= 2) deltaMin += 1440;  // Next day (skip if <2 min away)
    return static_cast<uint32_t>(deltaMin) * 60000UL;
}

// Check if current time is inside the sleep window (sleepTime..wakeTime)
bool isInSleepWindow() {
    const int16_t nowMin = static_cast<int16_t>(prtClock.getHour()) * 60
                         + prtClock.getMinute();
    const int16_t sleepMin = static_cast<int16_t>(Globals::sleepHour) * 60
                           + Globals::sleepMinute;
    const int16_t wakeMin = static_cast<int16_t>(Globals::wakeHour) * 60
                          + Globals::wakeMinute;
    // Window wraps midnight: sleep 00:11, wake 06:56
    if (sleepMin < wakeMin) {
        return nowMin >= sleepMin && nowMin < wakeMin;
    }
    // Window does NOT wrap: sleep 23:00, wake 06:00
    return nowMin >= sleepMin || nowMin < wakeMin;
}

// Calculate deep sleep duration in microseconds (from now until wake time)
uint64_t calcSleepDurationUs() {
    const int16_t nowMin = static_cast<int16_t>(prtClock.getHour()) * 60
                         + prtClock.getMinute();
    const int16_t wakeMin = static_cast<int16_t>(Globals::wakeHour) * 60
                          + Globals::wakeMinute;
    int16_t deltaMin = wakeMin - nowMin;
    if (deltaMin <= 0) deltaMin += 1440;
    return static_cast<uint64_t>(deltaMin) * 60ULL * 1000000ULL;
}

void enterDeepSleep() {
    const uint64_t durationUs = calcSleepDurationUs();
    const uint16_t durationMin = static_cast<uint16_t>(durationUs / 60000000ULL);
    PF("[Sleep] Entering deep sleep for %uu%02u (until %02u:%02u)\n",
       durationMin / 60, durationMin % 60, Globals::wakeHour, Globals::wakeMinute);

    // Cleanup: LEDs off
    FastLED.clear();
    FastLED.show();

    // Cleanup: audio stop
    audio.stop();

    // Cleanup: WiFi off
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);

    // Cleanup: SD unmount
    SD.end();

    Serial.flush();
    delay(50);

    esp_sleep_enable_timer_wakeup(durationUs);
    esp_deep_sleep_start();
}

void cb_enterDeepSleep();  // Forward declaration

void cb_sleepAfterTTS() {
    // Wait until TTS finishes, then enter deep sleep
    if (isSentencePlaying()) {
        timers.restart(SECONDS(1), 1, cb_sleepAfterTTS);
        return;
    }
    enterDeepSleep();
}

void cb_enterDeepSleep() {
    // Guard: boot cooldown — refuse sleep within 60s of boot
    // Prevents stale TCP retransmits from triggering immediate re-sleep
    if (millis() < sleepCooldownMs) {
        PF("[Sleep] Blocked — boot cooldown (%lus remaining)\n",
           (sleepCooldownMs - millis()) / 1000UL);
        return;  // Don't retry — armDeepSleep will re-arm later
    }
    // Guard: don't sleep mid-write or mid-playback
    if (AlertState::isSdBusy() || isFragmentPlaying()) {
        if (++sleepRetries <= maxSleepRetries) {
            PF("[Sleep] busy, retry %u/%u in 1 min\n", sleepRetries, maxSleepRetries);
            timers.restart(MINUTES(1), 1, cb_enterDeepSleep);
        } else {
            PL("[Sleep] still busy after 30 min — sleeping anyway");
            enterDeepSleep();
        }
        return;
    }
    // Stop any playing sentence, then speak "welterusten"
    PlaySentence::stop();
    PlaySentence::addTTS("welterusten");
    PL("[Sleep] TTS welterusten queued");
    // Wait for TTS to finish, then enter deep sleep
    timers.create(SECONDS(3), 1, cb_sleepAfterTTS);
}

void armDeepSleep() {
    if (!Globals::deepSleepEnabled) return;
    if (timers.isActive(cb_enterDeepSleep)) return;
    if (timers.isActive(cb_sleepAfterTTS)) return;
    if (!prtClock.isTimeFetched()) return;

    // After power cycle during sleep window: stay active (option B)
    // esp_sleep_get_wakeup_cause() is reliable; esp_reset_reason() is not
    const auto wakeReason = esp_sleep_get_wakeup_cause();
    static bool wakeLogged = false;
    if (!wakeLogged) {
        if (wakeReason == ESP_SLEEP_WAKEUP_TIMER)
            PL("[Sleep] Wake up by timer");
        else
            PL("[Sleep] Coldboot");
        wakeLogged = true;
    }
    if (wakeReason == ESP_SLEEP_WAKEUP_UNDEFINED && isInSleepWindow()) {
        PL("[Sleep] Power cycle during sleep window — staying active");
        return;
    }

    sleepRetries = 0;
    const uint32_t delayMs = calcMsUntilTime(Globals::sleepHour, Globals::sleepMinute);
    timers.create(delayMs, 1, cb_enterDeepSleep);
    Globals::sleepArmed = true;
    const uint16_t totalMin = static_cast<uint16_t>(delayMs / 60000UL);
    PF("[Sleep] Armed at %02u:%02u, in %uu%02u\n",
       Globals::sleepHour, Globals::sleepMinute, totalMin / 60, totalMin % 60);
}

// ─── Clock tick ─────────────────────────────────────────────

void cb_clockUpdate() {
    static uint8_t lastDay = 0;
    prtClock.update();

    // Arm daily reboot once clock is valid (idempotent)
    armDailyReboot();

    // Arm deep sleep once clock is valid (idempotent)
    armDeepSleep();

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
    // Don't try to start a new fragment while one is still playing
    if (Globals::tvMode && isFragmentPlaying()) {
        timers.restart(SECONDS(5), 1, cb_playFragment);
        return;
    }
    RunManager::requestPlayFragment();
    // Schedule next: TV sim uses short intervals, else normal logic
    uint32_t lo, hi;
    if (Globals::tvMode) {
        lo = Globals::tvAudioMinMs;
        hi = Globals::tvAudioMaxMs;
    } else {
        lo = AudioPolicy::effectiveFragmentMin();
        hi = AudioPolicy::effectiveFragmentMax();
        if (!AudioPolicy::isWebFragmentRangeActive()
            && AudioPolicy::themeBoxId().startsWith("web-")) {
            lo = Globals::singleDirMinIntervalMs;
            hi = Globals::singleDirMaxIntervalMs;
        }
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
    timers.create(webAudioNextFadeMs + 1U, 1, cb_playNextFragment);
}

void cb_playPendingFragment() {
    if (!hasPendingFragment) return;
    hasPendingFragment = false;
    if (!AudioPolicy::requestFragment(pendingFragment)) {
        RUN_LOG_WARN("[AudioRun] playback rejected\n");
    }
}

void cb_stopThenPlayPending() {
    constexpr uint16_t interruptFadeMs = 500U;
    PlayAudioFragment::stop(interruptFadeMs);
    timers.create(interruptFadeMs + 1U, 1, cb_playPendingFragment);
}

void cb_startSync() {
    PlayAudioFragment::stop(0);  // Immediate stop — no fade during sync
    AlertState::setSyncMode(true);
}

// ─── Web audio interval/silence support ─────────────────────

uint32_t webExpiryMs = Globals::defaultWebExpiryMs;
bool     pendingSilenceActive = false;

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

void RunManager::begin() {
    // Compute pre-granted capabilities from Stage0/1
    uint16_t preGranted = 0;
    if (hwStatus & HW_I2C) preGranted |= Cap::I2C;
    if (prtClock.isTimeFetched()) preGranted |= Cap::RTC;

    // Delegate to BootSequencer — the manifest drives everything
    BootSequencer::begin(preGranted);
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
    uint8_t targetFile = (file >= 0) ? file : 0;
    
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

float pendingAudioLevel = 1.0f;
void cb_applyAudioLevel();

void RunManager::requestSetAudioLevel(float value) {
    pendingAudioLevel = value;
    timers.cancel(cb_applyAudioLevel);
    timers.create(1, 1, cb_applyAudioLevel);
}

void cb_applyAudioLevel() {
    // F9 pattern: webMultiplier can be >1.0, no clamp
    audio.setVolumeWebMultiplier(pendingAudioLevel);
    // Arm/reset shared expiry — any web audio change resets countdown
    timers.cancel(cb_clearWebAudio);
    timers.create(webExpiryMs, 1, cb_clearWebAudio);
    RUN_LOG_INFO("[AudioRun] webMultiplier=%.2f\n",
                     static_cast<double>(pendingAudioLevel));
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

void cb_applySilence();

void cb_stopAudio() {
    PlayAudioFragment::stop(0);
    PlaySentence::stop();
    PL("[RunManager] Audio stopped for lux calibration");
}

void RunManager::requestStopAudio() {
    timers.cancel(cb_stopAudio);
    timers.create(1, 1, cb_stopAudio);
}

void cb_calModeTimeout() {
    if (Globals::luxCalibrationMode) {
        Globals::luxCalibrationMode = false;
        PL("[RunManager] Cal mode auto-cleared (no activity for 5 min)");
    }
}

void RunManager::touchCalActivity() {
    timers.restart(MINUTES(5), 1, cb_calModeTimeout);
}

void RunManager::requestSetSilence(bool active) {
    pendingSilenceActive = active;
    timers.cancel(cb_applySilence);
    timers.create(1, 1, cb_applySilence);
}

void cb_applySilence() {
    bool active = pendingSilenceActive;
    PF("[WebAudio] silence=%s\n", active ? "on" : "off");
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

    // Grant clock capability to BootSequencer (triggers dependent steps)
    if (!BootSequencer::isBootDone()) {
        BootSequencer::grant(Cap::CLOCK);
    }

    return true;
}

bool RunManager::isClockRunning() {
    return clockRunning;
}

bool RunManager::isClockInFallback() {
    return clockInFallback;
}

bool RunManager::requestSeedClockFromRtc() {
    static ClockRun clockRunSeed;
    return clockRunSeed.seedClockFromRtc(prtClock);
}

void RunManager::requestSyncRtcFromClock() {
    static ClockRun clockRunSync;
    clockRunSync.syncRtcFromClock(prtClock);
}

void RunManager::armAudioTimers() {
    // Create the periodic audio timers (first fire after random interval)
    timers.create(random(Globals::minSaytimeIntervalMs,
                         Globals::maxSaytimeIntervalMs + 1), 1, cb_sayTime);
    timers.create(random(Globals::minTemperatureSpeakIntervalMs,
                         Globals::maxTemperatureSpeakIntervalMs + 1), 1, cb_sayRTCtemperature);
    timers.create(random(Globals::minAudioIntervalMs,
                         Globals::maxAudioIntervalMs + 1), 1, cb_playFragment);
}

// Legacy resume functions — kept as no-ops for any stale callsites
void RunManager::resumeAfterSDBoot() {
    // Boot sequencer handles this now
}

void RunManager::resumeAfterWiFiBoot() {
    // Boot sequencer handles this now
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

// ─── TV Simulator ───────────────────────────────────────────

static uint8_t  tvHue = 0;
static uint8_t  tvSavedThemeDirs[MAX_THEME_DIRS];
static size_t   tvSavedThemeDirCount = 0;
static String   tvSavedThemeId;

void cb_tvScene();

void cb_tvTimeout() {
    RunManager::exitTvMode();
}

void cb_tvScene() {
    TvZoneTarget targets[TV_ZONES];

    for (int z = 0; z < TV_ZONES; z++) {
        CRGB color;
        uint8_t bri;
        bool instant = (random(100) < 35);  // 35% hard cut

        int roll = random(100);
        if (roll < 15) {
            // Dark/black zone — simulates edge of screen or scene cut
            color = CRGB::Black;
            bri = random(0, 15);
            instant = true;  // black is always instant
        } else if (roll < 65) {
            // Cool white/blue — typical TV glow
            uint8_t hue = random(140, 180);
            color = CHSV(hue, random(15, 70), random(200, 255));
            bri = random(140, 250);
        } else {
            // Colored scene — any hue
            tvHue += random(30, 120);
            color = CHSV(tvHue + random(0, 60), random(100, 220), random(150, 250));
            bri = random(80, 220);
        }

        targets[z].color      = color;
        targets[z].brightness = bri;
        targets[z].instant    = instant;
    }

    setTvZoneTargets(targets);
    timers.restart(random(100, 900), 1, cb_tvScene);
}

void RunManager::enterTvMode(uint8_t hours) {
    Globals::tvMode = true;

    // Save current (calendar) theme box before overwriting
    size_t savedCount = 0;
    const uint8_t* savedDirs = AudioPolicy::themeBoxDirs(savedCount);
    if (savedDirs && savedCount > 0) {
        memcpy(tvSavedThemeDirs, savedDirs, savedCount);
        tvSavedThemeDirCount = savedCount;
        tvSavedThemeId = AudioPolicy::themeBoxId();
    } else {
        tvSavedThemeDirCount = 0;
        tvSavedThemeId = "";
    }

    // Activate TVSIM theme box for audio
    const auto& boxes = GetAllThemeBoxes();
    for (const auto& box : boxes) {
        if (box.id == Globals::tvThemeBoxId) {
            std::vector<uint8_t> dirs8(box.entries.begin(), box.entries.end());
            AudioPolicy::setThemeBox(dirs8.data(), dirs8.size(), "tvsim");
            PF("[TvSim] Audio theme box %u (%s), %u dirs\n",
               box.id, box.name.c_str(), static_cast<unsigned>(dirs8.size()));
            break;
        }
    }

    FastLED.setBrightness(Globals::tvMaxBrightness);

    // Stop PNF calibration if running — it overwrites light patterns
    timers.cancel(LightRun::cb_pnfCalSample);
    timers.cancel(LightRun::cb_pnfCalNext);

    // Trigger first fragment quickly
    timers.restart(SECONDS(2), 1, cb_playFragment);

    tvHue = random(256);
    cb_tvScene();

    timers.create(hours * 3600000UL, 1, cb_tvTimeout);
    PF("[TvSim] Started for %u hours\n", hours);
}

void RunManager::exitTvMode() {
    Globals::tvMode = false;

    PlayAudioFragment::stop(500);

    // Restore pre-TvSim theme box (calendar)
    if (tvSavedThemeDirCount > 0) {
        AudioPolicy::setThemeBox(tvSavedThemeDirs, tvSavedThemeDirCount, tvSavedThemeId);
    } else {
        AudioPolicy::clearThemeBox();
    }

    LightRun::reapplyCurrentShow();

    timers.cancel(cb_tvScene);
    timers.cancel(cb_tvTimeout);

    PL("[TvSim] Stopped");
}

// ─── Deep Sleep API ─────────────────────────────────────────

void RunManager::requestDeepSleep() {
    if (millis() < sleepCooldownMs) {
        PF("[Sleep] Manual request blocked — boot cooldown (%lus)\n",
           (sleepCooldownMs - millis()) / 1000UL);
        return;
    }
    PL("[Sleep] Manual deep sleep requested");
    sleepRetries = 0;
    timers.restart(500, 1, cb_enterDeepSleep);
}

void RunManager::cancelDeepSleep() {
    timers.cancel(cb_enterDeepSleep);
    timers.cancel(cb_sleepAfterTTS);
    Globals::sleepArmed = false;
    PL("[Sleep] Cancelled");
}

// ─── Free-text TTS (download-to-SD, play via PlayFragment) ──

static String   freeTextTtsText;
static uint32_t freeTextTtsIntervalMs = 0;
static uint8_t  freeTextTtsRepeatCount = 0;
static uint32_t freeTextTtsDurationMs = 0;

static void cb_startWebFreeTextTts();
static void cb_webFreeTextRepeat();

static bool playFreeTextFromCache() {
    if (freeTextTtsDurationMs == 0) return false;
    AudioFragment frag{};
    frag.dirIndex   = Globals::ttsCacheDirIndex;
    frag.fileIndex  = Globals::ttsCacheFileIndex;
    frag.durationMs = freeTextTtsDurationMs;
    frag.fadeMs     = 500;
    strlcpy(frag.source, "freetext", sizeof(frag.source));
    return AudioPolicy::requestFragment(frag);
}

static void cb_webFreeTextRepeat() {
    playFreeTextFromCache();
}

static void cb_startWebFreeTextTts() {
    PF("[FreeText] \"%s\" interval=%ums repeat=%u\n",
       freeTextTtsText.c_str(), freeTextTtsIntervalMs, freeTextTtsRepeatCount);
    // Stop any playing audio before SD I/O
    if (isFragmentPlaying()) PlayAudioFragment::stop(0);
    if (isSentencePlaying()) PlaySentence::stop();

    uint32_t durationMs = PlaySentence::downloadTtsToCache(freeTextTtsText.c_str());
    if (durationMs == 0) {
        PF("[FreeText] Download failed, skipping\n");
        return;
    }
    freeTextTtsDurationMs = durationMs;

    playFreeTextFromCache();

    if (freeTextTtsRepeatCount > 0) {
        timers.create(freeTextTtsIntervalMs, freeTextTtsRepeatCount, cb_webFreeTextRepeat);
    }
}

void RunManager::requestClearWebFreeTextTts() {
    timers.cancel(cb_startWebFreeTextTts);
    timers.cancel(cb_webFreeTextRepeat);
    freeTextTtsText = "";
    freeTextTtsIntervalMs = 0;
    freeTextTtsRepeatCount = 0;
    freeTextTtsDurationMs = 0;
}

void RunManager::requestSetWebFreeTextTts(const String& text, uint32_t intervalMs, uint8_t repeatCount) {
    requestClearWebFreeTextTts();
    freeTextTtsText = text;
    freeTextTtsIntervalMs = intervalMs;
    freeTextTtsRepeatCount = repeatCount;
    // Defer all I/O to timer callback (web handler must be memory-only)
    timers.create(1, 1, cb_startWebFreeTextTts);
}

const String& RunManager::getWebFreeTextTtsText() {
    return freeTextTtsText;
}
