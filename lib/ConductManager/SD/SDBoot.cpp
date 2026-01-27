/**
 * @file SDBoot.cpp
 * @brief SD card one-time initialization implementation
 * @version 251231E
 * @date 2025-12-31
 *
 * Implements SD boot sequence with retry logic: attempts to mount SD card,
 * verifies version.txt matches firmware, shows pink/turquoise failure pattern
 * if SD mount fails, and reports status via NotifyConduct.
 * 
 * Index rebuild is deferred until valid time (RTC/NTP) available via
 * onTimeAvailable() callback from NotifyConduct - no polling needed.
 */

#include <Arduino.h>
#include "SDBoot.h"
#include "Globals.h"
#include "SDManager.h"
#include "SDPolicy.h"
#include "TimerManager.h"
#include "ConductManager.h"
#include "Notify/NotifyConduct.h"
#include "Notify/NotifyState.h"
#include "BootMaster.h"
#include <FastLED.h>

namespace {

constexpr int32_t  retryCount   = 3;     // 3 retries for SD mount
constexpr uint32_t retryIntervalMs = 500;

SDBoot* instance = nullptr;
bool loggedStart = false;
bool sdFailPatternActive = false;
bool rebuildPending = false;  // Deferred rebuild waiting for time
CRGB failLeds[NUM_LEDS];
uint8_t failPhase = 0;

// Forward declarations
static bool versionStringsEqual(const String& a, const char* b);

// Check if index rebuild is needed
static bool needsIndexRebuild() {
    auto& sdManager = SDManager::instance();
    
    if (!sdManager.fileExists(ROOT_DIRS)) {
        return true;
    }
    
    DirEntry dir;
    for (uint8_t i = 1; i <= SD_MAX_DIRS; i++) {
        if (sdManager.readDirEntry(i, &dir) && dir.fileCount > 0) {
            return false;  // Valid index exists
        }
    }
    return true;  // Empty index
}

// Timer callback for deferred rebuild
static void cb_deferredRebuild() {
    auto& sdManager = SDManager::instance();
    PF("[SDBoot] Rebuilding index with valid timestamps\n");
    sdManager.rebuildIndex();
    sdManager.setHighestDirNum();
    if (!sdManager.fileExists(WORDS_INDEX_FILE)) {
        PF("[SDBoot] Rebuilding %s\n", WORDS_INDEX_FILE);
        sdManager.rebuildWordsIndex();
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
    
    // Minimal FastLED init (normally done in LightManager)
    FastLED.addLeds<LED_TYPE, PIN_RGB, LED_RGB_ORDER>(failLeds, NUM_LEDS);
    FastLED.setBrightness(Globals::maxBrightness / 2);
    
    // Start pattern update timer (50ms = 20 FPS)
    timers().create(50, 0, cb_sdFailPattern);
    PL("[SDBoot] SD fail pattern started");
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
    auto& sdManager = SDManager::instance();
    if (!sdManager.begin(PIN_SD_CS, SPI, SPI_HZ)) {
        PF("[SDBoot] SD init failed.\n");
        SDManager::setReady(false);
        return;
    }
    
    // Version check
    if (sdManager.fileExists(SD_VERSION_FILENAME)) {
        File v = sdManager.openFileRead(SD_VERSION_FILENAME);
        String sdver = v ? v.readString() : "";
        if (v) {
            sdManager.closeFile(v);
        }
        if (!versionStringsEqual(sdver, SD_INDEX_VERSION)) {
            PF("[SDBoot][ERROR] SD version mismatch.\n");
            PF("  Card: %s\n  Need: %s\n", sdver.c_str(), SD_INDEX_VERSION);
            SDManager::setReady(false);
            return;  // Degraded mode - no HALT
        } else {
            PF("[SDBoot] SD version OK.\n");
        }
    } else {
        PF("[SDBoot] Version file missing.\n");
    }

    // SD mounted successfully - mark ready so boot can continue
    SDManager::setReady(true);
    hwStatus |= HW_SD;
    
    // Check if rebuild is needed
    if (needsIndexRebuild()) {
        // Defer rebuild until time available (via onTimeAvailable callback)
        rebuildPending = true;
        PF("[SDBoot] Index rebuild pending (waiting for RTC/NTP)\n");
    } else {
        // Existing valid index - use it
        PF("[SDBoot] Using existing valid index\n");
        sdManager.setHighestDirNum();
        if (!sdManager.fileExists(WORDS_INDEX_FILE)) {
            // Words index can be rebuilt without timestamp concern
            PF("[SDBoot] Rebuilding %s\n", WORDS_INDEX_FILE);
            sdManager.rebuildWordsIndex();
        }
    }
    
    // Load runtime config overrides from /config/globals.csv
    Globals::begin();
    
    // Restart boot timer with potentially updated bootPhaseMs
    bootMaster.restartBootTimer();
}

} // namespace

bool SDBoot::plan() {
    if (!instance) {
        instance = this;
    }

    // Success path
    if (SDManager::isReady()) {
        timers().cancel(cb_retryBoot);
        loggedStart = false;
        SDPolicy::showStatus();
        NotifyConduct::report(NotifyIntent::SD_OK);
        return true;
    }

    // Log once at start
    if (!loggedStart) {
        PL("[Conduct][Plan] SD boot starting");
        loggedStart = true;
    }

    // Try init
    initSD();

    // Check again after init attempt
    if (SDManager::isReady()) {
        timers().cancel(cb_retryBoot);
        loggedStart = false;
        SDPolicy::showStatus();
        NotifyConduct::report(NotifyIntent::SD_OK);
        return true;
    }

    // Arm retry timer (creates if not active)
    if (!timers().isActive(cb_retryBoot)) {
        timers().create(retryIntervalMs, retryCount, cb_retryBoot);
    }

    // Update retry status for WebGUI
    int32_t remaining = timers().getRepeatCount(cb_retryBoot);
    if (remaining > 0) {
        NotifyState::set(SC_SD, static_cast<uint8_t>(remaining));
    }

    // Still waiting for retries?
    if (timers().isActive(cb_retryBoot)) {
        return false;
    }

    // Timer exhausted - all retries done, still failed
    PL("[Conduct][Plan] SD boot failed after retries");
    loggedStart = false;
    SDPolicy::showStatus(true);
    NotifyConduct::report(NotifyIntent::SD_FAIL);
    startSdFailPattern();  // Pink/turquoise ambient mode
    return true;
}

void SDBoot::cb_retryBoot() {
    if (instance && instance->plan()) {
        ConductManager::resumeAfterSDBoot();
    }
}

void SDBoot::onTimeAvailable() {
    if (!rebuildPending) {
        return;
    }
    rebuildPending = false;
    // Defer rebuild to avoid blocking NTP_OK event processing
    timers().create(100, 1, cb_deferredRebuild);}