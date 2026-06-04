/**
 * @file BootSequencer.cpp
 * @brief Declarative boot sequence coordinator implementation
 * @version 260604B
 * @date 2026-06-04
 *
 * The manifest table describes the entire boot sequence.
 * Each step declares deps (requiresAll) and output (provides).
 * The sequencer evaluates the graph and starts steps as deps arrive.
 * When all steps are terminal → enterSteadyState() fires the verdict.
 */
#include <Arduino.h>
#include "BootSequencer.h"
#include "Cap.h"
#include "Globals.h"
#include "TimerManager.h"
#include "PRTClock.h"

// ─── Boot modules (init wrappers) ──────────────────────────
#include "BootManager.h"
#include "ContextController.h"
#include "Heartbeat/HeartbeatBoot.h"
#include "Heartbeat/HeartbeatRun.h"
#include "Status/StatusBoot.h"
#include "Status/StatusRun.h"
#include "Clock/ClockBoot.h"
#include "Clock/ClockRun.h"
#include "SD/SDBoot.h"
#include "SD/SDRun.h"
#include "WiFi/WiFiBoot.h"
#include "WiFi/WiFiRun.h"
#include "Web/WebBoot.h"
#include "Web/WebRun.h"
#include "Web/WebDirector.h"
#include "Sensors/SensorsBoot.h"
#include "Sensors/SensorsRun.h"
#include "Speak/SpeakBoot.h"
#include "Speak/SpeakRun.h"
#include "Calendar/CalendarBoot.h"
#include "Calendar/CalendarRun.h"
#include "Light/LightBoot.h"
#include "Light/LightRun.h"
#include "Light/LightPolicy.h"
#include "Audio/AudioBoot.h"
#include "Audio/AudioRun.h"
#include "Audio/AudioPolicy.h"
#include "Alert/AlertRun.h"
#include "Alert/AlertState.h"
#include "WebGuiStatus.h"
#include "RunManager.h"
#include "Tts/TtsTodoQueue.h"

// ─── Static storage ─────────────────────────────────────────

uint16_t   BootSequencer::granted_    = 0;
uint16_t   BootSequencer::failed_     = 0;
StepState  BootSequencer::states_[MAX_STEPS] = {};
uint8_t    BootSequencer::stepCount_  = 0;
bool       BootSequencer::verdictDone_ = false;

// ─── Step adapter instances ─────────────────────────────────
// Types with extern globals (heartbeatBoot, statusBoot, heartbeatRun,
// calendarBoot, calendarRun) are used via those globals.
// All others need a local instance.

static StatusRun     statusRun;
static ClockBoot     clockBoot;
static ClockRun      clockRun;
static SDBoot        sdBoot;
static SDRun         sdRun;
static WiFiBoot      wifiBoot;
static WiFiRun       wifiRun;
static WebBoot       webBoot;
static WebRun        webRun;
static SensorsBoot   sensorsBoot;
static SensorsRun    sensorsRun;
static SpeakBoot     speakBoot;
static SpeakRun      speakRun;
static LightBoot     lightBoot;
static LightRun      lightRun;
static AudioBoot     audioBoot;
static AudioRun      audioRun;

// ─── Step adapter functions ─────────────────────────────────
// Each wraps one Boot module. DONE = sync success.
// PENDING = async, module will call grant/fail later.

namespace {

StepResult initHeartbeat() {
    heartbeatBoot.plan();
    return StepResult::DONE;
}

StepResult initStatus() {
    statusBoot.plan();   // AlertRun::plan() → AlertState::reset()
    return StepResult::DONE;
}

StepResult initClockHw() {
    clockBoot.plan();    // ClockPolicy::begin() if RTC present
    return StepResult::DONE;
}

StepResult initSd() {
    // SDBoot::plan() tries SD mount + retries.
    // On success it calls BootSequencer::grant(Cap::SD | Cap::CONFIG).
    // On failure (retries exhausted) it calls BootSequencer::fail(Cap::SD).
    bool immediate = sdBoot.plan();
    return immediate ? StepResult::DONE : StepResult::PENDING;
}

StepResult initNetwork() {
    // WiFiBoot::plan() starts WiFi + CSV fetch timers.
    // Callbacks call BootSequencer::grant(Cap::WIFI) and grant(Cap::CSV).
    wifiBoot.plan();
    return StepResult::PENDING;
}

StepResult initWeb() {
    webBoot.plan();      // Start web server
    return StepResult::DONE;
}

StepResult initSensors() {
    sensorsBoot.plan();  // Init sensors (self-retry internally)
    return StepResult::DONE;
}

StepResult initSpeak() {
    speakBoot.plan();    // SpeakPolicy::configure()
    return StepResult::DONE;
}

StepResult initCalendar() {
    // CalendarBoot::plan() tries loading calendar + retries.
    // On success reports via AlertState. Sequencer polls AlertState.
    calendarBoot.plan();
    return AlertState::isCalendarOk() ? StepResult::DONE : StepResult::PENDING;
}

StepResult initLightHw() {
    lightBoot.plan();    // FastLED, LED map, 20fps timer
    return StepResult::DONE;
}

StepResult initAudioHw() {
    audioBoot.plan();    // I2S, volume, ping.wav
    return StepResult::DONE;
}

} // namespace

// ─── Manifest ───────────────────────────────────────────────
// THE boot sequence. Read this table to understand the entire boot.
//
// name          requiresAll                   provides                  init
// ─────────────────────────────────────────────────────────────────────────
static const BootStep manifest[] = {
    {"heartbeat",  0,                           0,                       initHeartbeat  },
    {"status",     0,                           0,                       initStatus     },
    {"rtc",        0,                           0,                       initClockHw    },
    {"sd",         0,                           Cap::SD | Cap::CONFIG,   initSd         },
    {"wifi",       0,                           Cap::WIFI | Cap::CSV,    initNetwork    },
    {"web",        0,                           Cap::WEB,                initWeb        },
    {"sensors",    0,                           Cap::SENSORS,            initSensors    },
    {"speak",      0,                           Cap::SPEAK,              initSpeak      },
    {"calendar",   Cap::SD | Cap::CLOCK | Cap::CSV, Cap::CALENDAR,      initCalendar   },
    {"RGBs",      Cap::SD | Cap::CONFIG,       Cap::LIGHT_HW,          initLightHw    },
    {"audio",     Cap::SD | Cap::CONFIG,       Cap::AUDIO_HW,          initAudioHw    },
};

static constexpr uint8_t MANIFEST_SIZE = sizeof(manifest) / sizeof(manifest[0]);

// ─── Sequencer tick ─────────────────────────────────────────

void BootSequencer::cb_evaluateSteps() {
    // Poll RUNNING steps that depend on external events
    for (uint8_t i = 0; i < stepCount_; i++) {
        if (states_[i] != StepState::RUNNING) continue;

        // Check if the step's capability was externally granted
        uint16_t provides = manifest[i].provides;
        if (provides != 0 && (granted_ & provides) == provides) {
            states_[i] = StepState::DONE;
            PF("[Boot] \xE2\x9C\x93 %s\n", manifest[i].name);
        }
    }

    // Special: calendar step polls AlertState (has its own retry logic)
    for (uint8_t i = 0; i < stepCount_; i++) {
        if (states_[i] != StepState::RUNNING) continue;
        if (manifest[i].provides == Cap::CALENDAR && AlertState::isCalendarOk()) {
            states_[i] = StepState::DONE;
            granted_ |= Cap::CALENDAR;
            PF("[Boot] \xE2\x9C\x93 %s\n", manifest[i].name);
        }
    }

    evaluate();
}

// ─── Boot timeout ───────────────────────────────────────────

void BootSequencer::cb_expireBoot() {
    if (verdictDone_) return;
    PL("[Boot] Timeout reached \u2014 forcing verdict");
    for (uint8_t i = 0; i < stepCount_; i++) {
        if (states_[i] == StepState::WAITING || states_[i] == StepState::RUNNING) {
            PF("[Boot] \u23F0 %s\n", manifest[i].name);
            states_[i] = StepState::FAILED;
        }
    }
    enterSteadyState();
}

// ─── Core sequencer logic ───────────────────────────────────

void BootSequencer::begin(uint16_t preGranted) {
    granted_ = preGranted;
    failed_ = 0;
    verdictDone_ = false;
    stepCount_ = MANIFEST_SIZE;

    for (uint8_t i = 0; i < MAX_STEPS; i++) {
        states_[i] = (i < stepCount_) ? StepState::WAITING : StepState::DONE;
    }

    // BootManager handles NTP/fallback clock; runs independently
    bootManager.begin();
    ContextController::begin();

    // Boot timeout — forces verdict regardless of step state
    timers.create(Globals::bootTimeoutMs, 1, cb_expireBoot);

    // Tick timer — evaluates waiting steps, polls running steps
    timers.create(500, 0, cb_evaluateSteps);

    // First evaluate immediately
    evaluate();
}

void BootSequencer::evaluate() {
    if (verdictDone_) return;

    bool changed = true;
    while (changed) {
        changed = false;
        for (uint8_t i = 0; i < stepCount_; i++) {
            // Skip terminal states
            if (states_[i] == StepState::DONE || states_[i] == StepState::FAILED) {
                continue;
            }

            // RUNNING: check if provides-cap was externally granted
            if (states_[i] == StepState::RUNNING) {
                uint16_t provides = manifest[i].provides;
                if (provides != 0 && (granted_ & provides) == provides) {
                    states_[i] = StepState::DONE;
                    changed = true;
                }
                continue;
            }

            // WAITING: check deps
            if (states_[i] == StepState::WAITING) {
                uint16_t req = manifest[i].requiresAll;
                if (req != 0 && (granted_ & req) != req) {
                    // Check if any required cap has failed — cascade
                    if ((failed_ & req) != 0) {
                        states_[i] = StepState::FAILED;
                        failed_ |= manifest[i].provides;
                        PF("[Boot] \xE2\x9C\x97 %s (dep)\n", manifest[i].name);
                        changed = true;
                    }
                    continue;  // Deps not met yet
                }

                // Deps met → mark RUNNING first to prevent re-entrancy
                // (init() may call grant() which calls evaluate() recursively)
                states_[i] = StepState::RUNNING;
                StepResult result = manifest[i].init();

                switch (result) {
                    case StepResult::DONE:
                        states_[i] = StepState::DONE;
                        granted_ |= manifest[i].provides;
                        changed = true;
                        break;
                    case StepResult::PENDING:
                        states_[i] = StepState::RUNNING;
                        PF("[Boot] ~ %s\n", manifest[i].name);
                        break;
                    case StepResult::FAILED:
                        states_[i] = StepState::FAILED;
                        failed_ |= manifest[i].provides;
                        PF("[Boot] \xE2\x9C\x97 %s\n", manifest[i].name);
                        changed = true;
                        break;
                }
            }
        }
    }

    checkBootDone();
}

void BootSequencer::grant(uint16_t caps) {
    if (verdictDone_) return;
    uint16_t newCaps = caps & ~granted_;
    if (newCaps == 0) return;  // Already have these

    granted_ |= caps;
    evaluate();
}

void BootSequencer::fail(uint16_t caps) {
    if (verdictDone_) return;
    failed_ |= caps;

    // Mark RUNNING steps that provide the failed cap as FAILED
    for (uint8_t i = 0; i < stepCount_; i++) {
        if (states_[i] == StepState::RUNNING &&
            manifest[i].provides != 0 &&
            (manifest[i].provides & caps) != 0) {
            states_[i] = StepState::FAILED;
            PF("[Boot] \xE2\x9C\x97 %s\n", manifest[i].name);
        }
    }

    // Cascade: steps waiting on failed caps also fail
    cascadeFailure(caps);
    evaluate();
}

void BootSequencer::cascadeFailure(uint16_t failedCaps) {
    bool changed = true;
    while (changed) {
        changed = false;
        for (uint8_t i = 0; i < stepCount_; i++) {
            if (states_[i] != StepState::WAITING) continue;
            uint16_t req = manifest[i].requiresAll;
            if (req != 0 && (failedCaps & req) != 0) {
                states_[i] = StepState::FAILED;
                failedCaps |= manifest[i].provides;
                failed_ |= manifest[i].provides;
                PF("[Boot] \xE2\x9C\x97 %s (dep)\n", manifest[i].name);
                changed = true;
            }
        }
    }
}

void BootSequencer::checkBootDone() {
    if (verdictDone_) return;

    for (uint8_t i = 0; i < stepCount_; i++) {
        if (states_[i] == StepState::WAITING || states_[i] == StepState::RUNNING) {
            return;  // Not all terminal
        }
    }

    // All steps are terminal — fire verdict
    enterSteadyState();
}

bool BootSequencer::has(uint16_t caps) {
    return (granted_ & caps) == caps;
}

bool BootSequencer::isBootDone() {
    return verdictDone_;
}

uint16_t BootSequencer::granted() {
    return granted_;
}

const char* BootSequencer::capName(uint16_t singleCap) {
    switch (singleCap) {
        case Cap::I2C:      return "I2C";
        case Cap::RTC:      return "RTC";
        case Cap::SD:       return "SD";
        case Cap::CONFIG:   return "CONFIG";
        case Cap::WIFI:     return "WIFI";
        case Cap::WEB:      return "WEB";
        case Cap::NTP:      return "NTP";
        case Cap::CLOCK:    return "CLOCK";
        case Cap::CSV:      return "CSV";
        case Cap::SENSORS:  return "SENSORS";
        case Cap::SPEAK:    return "SPEAK";
        case Cap::LIGHT_HW: return "LIGHT_HW";
        case Cap::AUDIO_HW: return "AUDIO_HW";
        case Cap::CALENDAR: return "CALENDAR";
        default:            return "?";
    }
}

// ─── Verdict: enterSteadyState ──────────────────────────────
// Reads granted_ bitmask and starts only what's available.
// This replaces resumeAfterSDBoot() + resumeAfterWiFiBoot().

void BootSequencer::enterSteadyState() {
    if (verdictDone_) return;
    verdictDone_ = true;

    // Cancel sequencer timers
    timers.cancel(cb_evaluateSteps);
    timers.cancel(cb_expireBoot);

    bool hasSd       = has(Cap::SD);
    bool hasWifi     = has(Cap::WIFI);
    bool hasClock    = has(Cap::CLOCK);
    bool hasCsv      = has(Cap::CSV);
    bool hasLightHw  = has(Cap::LIGHT_HW);
    bool hasAudioHw  = has(Cap::AUDIO_HW);
    bool hasCalendar = has(Cap::CALENDAR);
    bool hasFull     = hasSd && hasWifi && hasClock && hasCsv;

    // ─── Log verdict (only when degraded) ─────────────
    if (!hasFull || !hasLightHw || !hasAudioHw || !hasCalendar) {
        PL("[Boot] DEGRADED:");
        if (!hasSd)       PL("  - No SD");
        if (!hasWifi)     PL("  - No WiFi");
        if (!hasClock)    PL("  - No clock");
        if (!hasCsv)      PL("  - No NAS data");
        if (!hasLightHw)  PL("  - No RGBs");
        if (!hasAudioHw)  PL("  - No audio");
        if (!hasCalendar) PL("  - No calendar");
    }

    // ─── Start runtime layers (only what's available) ───
    // These are the "feestelijkheden" — they depend on the verdict.

    // Always: heartbeat run, status run, clock run, sensors run
    heartbeatRun.plan();
    statusRun.plan();
    clockRun.plan();
    sensorsRun.plan();
    speakRun.plan();
    wifiRun.plan();

    // SD-dependent run layers
    if (hasSd) {
        sdRun.plan();
        webRun.plan();
        WebDirector::instance().plan();
    }

    // Calendar run (has its own retry if calendar not loaded yet)
    if (hasSd && hasClock) {
        calendarRun.plan();
    }

    // Light runtime: rotation timers, lux measurement, shifts
    if (hasLightHw) {
        // PNF calibration is handled inside LightRun::plan()
        // If uncalibrated patterns exist, plan() defers rotation timers
        // until calibration completes. This runs as a background task
        // — it does NOT block audio, calendar, or TTS.
        lightRun.plan();
    }

    // Audio runtime: fragment timer, sayTime, sayTemp
    if (hasAudioHw) {
        audioRun.plan();
        RunManager::armAudioTimers();
    }

    // Welcome and boot fragment
    if (hasCalendar && hasAudioHw) {
        AlertRun::playWelcomeIfPending();
        RunManager::triggerBootFragment();
    } else if (hasWifi && hasAudioHw) {
        // No calendar but TTS available — still say welcome
        AlertRun::playWelcomeIfPending();
    }

    // Signal runtime start (stops boot phase flashing, starts reminders)
    AlertRun::report(AlertRequest::START_RUNTIME);

    // Self-consuming TTS render-queue (boots only if SD+WiFi available)
    if (hasSd && hasWifi) {
        TtsTodoQueue::plan();
    }

    PL("[Boot] \xE2\x9C\x93 Boot");
}
