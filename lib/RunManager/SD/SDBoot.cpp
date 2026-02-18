/**
 * @file SDBoot.cpp
 * @brief SD card one-time initialization implementation
 * @version 260218N
 * @date 2026-02-18
 */
#include <Arduino.h>
#include "SDBoot.h"
#include "Globals.h"
#include "SDController.h"
#include "SDPolicy.h"
#include "TimerManager.h"
#include "RunManager.h"
#include "Alert/AlertRun.h"
#include "Alert/AlertState.h"
#include "BootManager.h"
#include <FastLED.h>

namespace {

constexpr uint8_t  retryCount   = 3;     // 3 retries for SD mount
constexpr uint32_t retryIntervalMs = 500;

SDBoot* instance = nullptr;
bool loggedStart = false;
bool sdFailPatternActive = false;
bool rebuildPending = false;  // Deferred rebuild waiting for time
bool versionMismatch = false; // SD readable but index version wrong
static uint8_t pendingSyncDir = 0;  // Dir number awaiting syncDirectory (0 = none)
CRGB failLeds[NUM_LEDS];
uint8_t failPhase = 0;

// Forward declarations
static bool versionStringsEqual(const String& a, const char* b);

// Check if index rebuild is needed
static bool needsIndexRebuild() {
    if (!SDController::fileExists(ROOT_DIRS)) {
        return true;
    }
    
    DirEntry dir;
    for (uint8_t i = 1; i <= SD_MAX_DIRS; i++) {
        if (SDController::readDirEntry(i, &dir) && dir.fileCount > 0) {
            return false;  // Valid index exists
        }
    }
    return true;  // Empty index
}

// Timer callback for deferred rebuild
static void cb_deferredRebuild() {
    PF("[SDBoot] Rebuilding index, existing votes will be preserved\n");
    SDController::rebuildIndex();
    SDController::updateHighestDirNum();
    if (!SDController::fileExists(WORDS_INDEX_FILE)) {
        PF("[SDBoot] Rebuilding %s\n", WORDS_INDEX_FILE);
        SDController::rebuildWordsIndex();
    }
}

// SD fail ambient pattern: pink ↔ turquoise crossfade
void cb_sdFailPattern() {
    failPhase++;
    uint8_t blend = sin8(failPhase);
    uint8_t hue = lerp8by8(245, 128, blend);  // Pink(245) ↔ Aqua(128)
    uint8_t sat = 200;
    uint8_t val = 77 + (sin8(failPhase * 2) >> 2);  // 30%-55% brightness
    
    CHSV color(hue, sat, val);
    fill_solid(failLeds, NUM_LEDS, color);
    FastLED.show();
}

void startSdFailPattern() {
    if (sdFailPatternActive) return;
    sdFailPatternActive = true;
    
    // Minimal FastLED init (normally done in LightController)
    FastLED.addLeds<LED_TYPE, PIN_RGB, LED_RGB_ORDER>(failLeds, NUM_LEDS);
    FastLED.setBrightness(Globals::maxBrightness / 2);
    
    // Start pattern update timer (50ms = 20 FPS)
    timers.create(50, 0, cb_sdFailPattern);
    PL("[SDBoot] FAILED_SD pattern started");
}

// Version string comparison (whitespace insensitive)
static bool versionStringsEqual(const String& a, const char* b) {
    int ia = 0, ib = 0;
    while (a[ia] != 0 && b[ib] != 0) {
        while (a[ia] == '\r' || a[ia] == '\n' || a[ia] == ' ' || a[ia] == '\t') {
            ia++;
        }
        while (b[ib] == '\r' || b[ib] == '\n' || b[ib] == ' ' || b[ib] == '\t') {
            ib++;
        }
        if (a[ia] != b[ib]) {
            return false;
        }
        if (a[ia] == 0 || b[ib] == 0) {
            break;
        }
        ia++;
        ib++;
    }
    while (a[ia] == '\r' || a[ia] == '\n' || a[ia] == ' ' || a[ia] == '\t') {
        ia++;
    }
    while (b[ib] == '\r' || b[ib] == '\n' || b[ib] == ' ' || b[ib] == '\t') {
        ib++;
    }
    return (a[ia] == 0 && b[ib] == 0);
}

// Initialize SD card and validate index
static void initSD() {
    if (!SDController::begin(PIN_SD_CS, SPI, SPI_HZ)) {
        PF("[SDBoot] SD init failed.\n");
        SDController::setReady(false);
        return;
    }
    
    // Version check
    if (SDController::fileExists(SD_VERSION_FILENAME)) {
        File v = SDController::openFileRead(SD_VERSION_FILENAME);
        String sdver = v ? v.readString() : "";
        if (v) {
            SDController::closeFile(v);
        }
        if (!versionStringsEqual(sdver, SD_INDEX_VERSION)) {
            PF("[SDBoot] SD readable but index version mismatch\n");
            PF("  Card: %s\n  Need: %s\n", sdver.c_str(), SD_INDEX_VERSION);
            versionMismatch = true;
            SDController::setReady(false);
            return;
        } else {
            PF_BOOT("[SDBoot] version OK\n");
        }
    } else {
        PF("[SDBoot] Version file missing\n");
    }

    // SD mounted successfully - mark ready so boot can continue
    SDController::setReady(true);
    hwStatus |= HW_SD;
    
    // Check if rebuild is needed
    if (needsIndexRebuild()) {
        // Defer rebuild until time available (via onTimeAvailable callback)
        rebuildPending = true;
        PF("[SDBoot] Index rebuild pending (waiting for RTC/NTP)\n");
    } else {
        // Existing valid index - use it
        PF_BOOT("[SDBoot] index valid\n");
        SDController::updateHighestDirNum();
        if (!SDController::fileExists(WORDS_INDEX_FILE)) {
            // Words index can be rebuilt without timestamp concern
            PF("[SDBoot] Rebuilding %s\n", WORDS_INDEX_FILE);
            SDController::rebuildWordsIndex();
        }
    }
    
    // Load runtime config overrides from /config/globals.csv
    Globals::begin();
    
    // Restart boot timer with potentially updated bootPhaseMs
    bootManager.restartBootTimer();
}

/// Report SD OK (logging + alerts only, no boot control)
static void reportSdOk() {
    loggedStart = false;
    SDPolicy::showStatus();
    AlertRun::report(AlertRequest::SD_OK);
}

/// Report SD failure (logging + alerts + fail pattern, no boot control)
static void reportSdFail() {
    if (versionMismatch) {
        PL("[SDBoot] SD readable but version mismatch — upload correct firmware or re-index");
    } else {
        PL("[SDBoot] SD boot failed after retries");
    }
    loggedStart = false;
    SDPolicy::showStatus(true);
    AlertRun::report(AlertRequest::SD_FAIL);
    startSdFailPattern();
}

} // namespace

bool SDBoot::plan() {
    if (!instance) {
        instance = this;
    }

    // Already OK?
    if (AlertState::isSdOk()) {
        reportSdOk();
        RunManager::resumeAfterSDBoot();
        return true;
    }

    // Log once at start
    if (!loggedStart) {
        PL_BOOT("[SDBoot] starting");
        loggedStart = true;
    }

    // First attempt
    initSD();

    if (AlertState::isSdOk()) {
        reportSdOk();
        RunManager::resumeAfterSDBoot();
        return true;
    }

    // Failed — arm retry timer (let it count down naturally)
    timers.create(retryIntervalMs, retryCount, cb_retryBoot);
    return false;
}

void SDBoot::cb_retryBoot() {
    if (!instance) return;

    // Already OK? (e.g., another path initialized SD)
    if (AlertState::isSdOk()) {
        timers.cancel(cb_retryBoot);
        reportSdOk();
        RunManager::resumeAfterSDBoot();
        return;
    }

    auto remaining = timers.remaining();
    AlertState::set(SC_SD, remaining);

    // Try init
    initSD();

    if (AlertState::isSdOk()) {
        timers.cancel(cb_retryBoot);
        reportSdOk();
        RunManager::resumeAfterSDBoot();
        return;
    }

    // Last retry exhausted?
    if (remaining <= 1) {
        reportSdFail();
        RunManager::resumeAfterSDBoot();  // Continue boot without SD
    }
}

void SDBoot::onTimeAvailable() {
    if (!rebuildPending) {
        return;
    }
    rebuildPending = false;
    // Defer rebuild to avoid blocking NTP_OK event flow
    timers.create(100, 1, cb_deferredRebuild);
}

void SDBoot::requestRebuild() {
    if (timers.isActive(cb_deferredRebuild)) {
        PF("[SDBoot] Rebuild already scheduled\n");
        return;
    }
    timers.create(100, 1, cb_deferredRebuild);
    PF("[SDBoot] Rebuild requested\n");
}

static void cb_deferredSyncDir() {
    uint8_t dir = pendingSyncDir;
    pendingSyncDir = 0;
    if (dir == 0) return;
    SDController::lockSD();
    SDController::syncDirectory(dir);
    SDController::updateHighestDirNum();
    SDController::unlockSD();
}

void SDBoot::requestSyncDir(uint8_t dirNum) {
    if (dirNum == 0) return;
    pendingSyncDir = dirNum;
    timers.create(100, 1, cb_deferredSyncDir);
    PF("[SDBoot] SyncDir %03u requested\n", dirNum);
}

bool SDBoot::isVersionMismatch() {
    return versionMismatch;
}