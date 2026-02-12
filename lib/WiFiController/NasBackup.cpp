/**
 * @file NasBackup.cpp
 * @brief Push pattern/color CSVs to NAS csv_server.py after save
 * @version 260212I
 * @date 2026-02-12
 *
 * Safe push design:
 *   requestPush(filename) sets a pending bool and starts a repeating timer.
 *   Each tick the timer checks: WiFi ok, NAS ok, SD free, audio idle.
 *   If safe → read file from SD, POST to NAS (short timeout).
 *   On success → clear pending; if nothing pending → cancel timer.
 *
 * Health check:
 *   checkHealth() probes GET /api/health with a 1.5s timeout.
 *   Called once from WiFiBoot after connect, then every 57 minutes.
 */
#include <Arduino.h>

#include "NasBackup.h"
#include "Globals.h"
#include "TimerManager.h"
#include "SDController.h"
#include "Alert/AlertState.h"
#include "AudioState.h"
#include <HTTPClient.h>
#include <WiFiClient.h>

extern TimerManager timers;

namespace {

bool pendingPatterns = false;
bool pendingColors   = false;
constexpr uint32_t PUSH_INTERVAL_MS  = 10000;  // retry every 10s
constexpr uint32_t HEALTH_INTERVAL_MS = 57UL * 60UL * 1000UL;  // 57 minutes
constexpr uint32_t NAS_TIMEOUT_MS    = 1500;   // short timeout

// ─────────────────────────────────────────────────────────────
// Build server root from csvBaseUrl (strip "/csv/" suffix)
// ─────────────────────────────────────────────────────────────
String serverRoot() {
    String base(Globals::csvBaseUrl);
    int csvIdx = base.indexOf("/csv");
    if (csvIdx > 0) base = base.substring(0, csvIdx);
    return base;
}

// ─────────────────────────────────────────────────────────────
// Read file from SD into a String (caller must ensure SD is free)
// ─────────────────────────────────────────────────────────────
bool readFileFromSD(const char* path, String& content) {
    if (!AlertState::isSdOk()) return false;
    SDController::lockSD();
    File file = SDController::openFileRead(path);
    if (!file) { SDController::unlockSD(); return false; }

    size_t fileSize = file.size();
    if (fileSize == 0 || fileSize > 65536) {
        SDController::closeFile(file);
        SDController::unlockSD();
        return false;
    }

    content.reserve(fileSize);
    while (file.available()) {
        content += (char)file.read();
    }
    SDController::closeFile(file);
    SDController::unlockSD();
    return true;
}

// ─────────────────────────────────────────────────────────────
// POST file content to NAS csv_server.py
// ─────────────────────────────────────────────────────────────
bool postToNas(const char* filename, const String& content) {
    String url = serverRoot();
    url += "/api/upload?file=";
    url += filename;

    HTTPClient http;
    WiFiClient client;
    http.begin(client, url);
    http.addHeader("Content-Type", "text/csv");
    http.setTimeout(NAS_TIMEOUT_MS);

    int httpCode = http.POST(content);
    http.end();

    if (httpCode == 200) {
        PF("[NasBackup] %s pushed OK\n", filename);
        return true;
    }
    PF("[NasBackup] %s push failed: HTTP %d\n", filename, httpCode);
    return false;
}

// ─────────────────────────────────────────────────────────────
// Push one pending file if safe. Returns true if it pushed.
// ─────────────────────────────────────────────────────────────
bool pushOnePending() {
    const char* filename = nullptr;
    const char* sdPath   = nullptr;
    bool* flag           = nullptr;

    if (pendingPatterns) {
        filename = "light_patterns.csv";
        sdPath   = "/light_patterns.csv";
        flag     = &pendingPatterns;
    } else if (pendingColors) {
        filename = "light_colors.csv";
        sdPath   = "/light_colors.csv";
        flag     = &pendingColors;
    }
    if (!flag) return false;

    String content;
    if (!readFileFromSD(sdPath, content)) return false;
    if (postToNas(filename, content)) {
        *flag = false;
        return true;
    }
    return false;
}

// ─────────────────────────────────────────────────────────────
// Timer callback — retry push when safe
// ─────────────────────────────────────────────────────────────
void cb_pushToNas() {
    if (!AlertState::isWifiOk()) return;
    if (!AlertState::isNasOk())  return;
    if (AlertState::isSdBusy())  return;
    if (isAudioBusy())           return;

    pushOnePending();

    // If nothing pending, cancel timer
    if (!pendingPatterns && !pendingColors) {
        timers.cancel(cb_pushToNas);
    }
}

// ─────────────────────────────────────────────────────────────
// Timer callback — periodic NAS health probe
// ─────────────────────────────────────────────────────────────
void cb_checkNasHealth() {
    NasBackup::checkHealth();
}

} // namespace

// ─────────────────────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────────────────────
void NasBackup::requestPush(const char* filename) {
    if (strcmp(filename, "light_patterns.csv") == 0) {
        pendingPatterns = true;
    } else if (strcmp(filename, "light_colors.csv") == 0) {
        pendingColors = true;
    } else {
        return;
    }
    // Start repeating timer if not already running
    timers.create(PUSH_INTERVAL_MS, 0, cb_pushToNas);
}

void NasBackup::checkHealth() {
    if (!AlertState::isWifiOk()) return;

    String url = serverRoot();
    url += "/api/health";

    HTTPClient http;
    WiFiClient client;
    http.begin(client, url);
    http.setTimeout(NAS_TIMEOUT_MS);

    int httpCode = http.GET();
    http.end();

    bool ok = (httpCode == 200);
    AlertState::setNasStatus(ok);
    if (!ok) {
        PF("[NasBackup] NAS unreachable (HTTP %d)\n", httpCode);
    }

    // Start periodic recheck (57 min) if not already running
    timers.create(HEALTH_INTERVAL_MS, 0, cb_checkNasHealth);
}
