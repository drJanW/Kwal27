/**
 * @file AlertRGB.cpp
 * @brief RGB LED status flash coordination implementation
 * @version 260104M
 * @date 2026-01-04
 *
 * Implements status flash sequence for hardware errors.
 *
 * Flash burst timing: black(1s) + color(1-2s) + black(1s) ≈ 3-4s per component.
 * Boot sequence: 2× bursts immediately when error detected.
 * Reminders: Single flash at growing intervals (2, 20, 200, 2000... min).
 *
 * Hardware presence: Only components marked as present in HWconfig.h
 * (*_PRESENT=true) trigger flashes. Absent hardware is silently skipped.
 *
 * IMPORTANT: Uses TimerManager::restart() for sequence steps because the
 * same callback (cb_sequenceStep) is reused with different durations.
 * Using create() would fail silently since the timer already exists.
 */

#define LOCAL_LOG_LEVEL LOG_LEVEL_INFO
#include "AlertRGB.h"
#include "AlertPolicy.h"
#include "LightManager.h"
#include "Light/LightRun.h"
#include "TimerManager.h"
#include "ContextFlags.h"
#include "ContextStatus.h"
#include "Globals.h"
#include <FastLED.h>

namespace {

bool flashing = false;

// Sequence state machine
uint8_t sequenceStep = 0;
uint64_t cachedNotOkBits = 0;

// Step types: 0=black, 1=color
struct FlashStep {
    uint32_t color;
    uint32_t durationMs;
};

constexpr uint8_t MAX_STEPS = 20;
FlashStep steps[MAX_STEPS];
uint8_t stepCount = 0;

void applySolid(uint32_t color) {
    CRGB c((color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF);
    PlayLightShow(MakeSolidParams(c));
}

void addStep(uint32_t color, uint32_t durationMs) {
    if (stepCount < MAX_STEPS) {
        steps[stepCount].color = color;
        steps[stepCount].durationMs = durationMs;
        stepCount++;
    }
}

void cb_sequenceStep();

void scheduleNextStep() {
    if (sequenceStep >= stepCount) {
        // Done - restore show
        flashing = false;
        LightRun::reapplyCurrentShow();
        return;
    }
    
    applySolid(steps[sequenceStep].color);
    uint32_t duration = steps[sequenceStep].durationMs;
    sequenceStep++;
    
    // Use restart() - timer may already exist from previous step
    timers.restart(duration, 1, cb_sequenceStep);
}

void cb_sequenceStep() {
    scheduleNextStep();
}

bool isNotOk(uint32_t statusBit) {
    return cachedNotOkBits & (1ULL << statusBit);
}

void buildSequence() {
    stepCount = 0;
    sequenceStep = 0;
    
    // Initial black
    addStep(0x000000, Globals::flashNormalMs);
    
    // SD and WiFi always required
    if (isNotOk(STATUS_SD_OK)) {
        addStep(AlertPolicy::COLOR_SD, Globals::flashCriticalMs);
        addStep(0x000000, Globals::flashNormalMs);
    }
    if (isNotOk(STATUS_WIFI_OK)) {
        addStep(AlertPolicy::COLOR_WIFI, Globals::flashCriticalMs);
        addStep(0x000000, Globals::flashNormalMs);
    }
    // Optional hardware: only flash if PRESENT
    if (RTC_PRESENT && isNotOk(STATUS_RTC_OK)) {
        addStep(AlertPolicy::COLOR_RTC, Globals::flashNormalMs);
        addStep(0x000000, Globals::flashNormalMs);
    }
    if (isNotOk(STATUS_NTP_OK)) {
        addStep(AlertPolicy::COLOR_NTP, Globals::flashNormalMs);
        addStep(0x000000, Globals::flashNormalMs);
    }
    if (DISTANCE_SENSOR_PRESENT && isNotOk(STATUS_DISTANCE_SENSOR_OK)) {
        addStep(AlertPolicy::COLOR_DISTANCE_SENSOR, Globals::flashNormalMs);
        addStep(0x000000, Globals::flashNormalMs);
    }
    if (LUX_SENSOR_PRESENT && isNotOk(STATUS_LUX_SENSOR_OK)) {
        addStep(AlertPolicy::COLOR_LUX_SENSOR, Globals::flashNormalMs);
        addStep(0x000000, Globals::flashNormalMs);
    }
    if (SENSOR3_PRESENT && isNotOk(STATUS_SENSOR3_OK)) {
        addStep(AlertPolicy::COLOR_SENSOR3, Globals::flashNormalMs);
        addStep(0x000000, Globals::flashNormalMs);
    }
}

void cb_flash() {
    // Cancel any running sequence timer from previous burst
    timers.cancel(cb_sequenceStep);
    
    cachedNotOkBits = ContextFlags::getHardwareFailBits();
    if (cachedNotOkBits == 0) {
        flashing = false;
        LightRun::reapplyCurrentShow();
        return;
    }

    flashing = true;
    PF("[AlertRGB] flash burst start bits=0x%llX\n", cachedNotOkBits);
    
    buildSequence();
    scheduleNextStep();
}

} // namespace

namespace AlertRGB {

void startFlashing() {
    // Flash burst: configurable via globals.csv
    // Interval between bursts, repeats, and optional growth factor for exponential backoff
    // Only start if not already flashing (create fails if timer exists)
    if (timers.create(Globals::flashBurstIntervalMs, Globals::flashBurstRepeats, cb_flash, Globals::flashBurstGrowth)) {
        cb_flash();  // immediate first flash only if newly created
    }
}

void stopFlashing() {
    timers.cancel(cb_flash);
    applySolid(0);
    LightRun::reapplyCurrentShow();
}

bool isFlashing() {
    return flashing;
}

} // namespace AlertRGB