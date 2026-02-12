/**
 * @file NasBackup.cpp
 * @brief Push pattern/color CSVs to NAS csv_server.py after save
 * @version 260213B
 * @date 2026-02-13
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
 *   On failure: fast retries every 2 min (max 3). If exhausted,
 *   reconnects WiFi to reset the TCP/IP stack and retries once more.
 *   If still unreachable, resumes the 57-minute slow cycle.
 */
#include <Arduino.h>

#include "NasBackup.h"
#include "Globals.h"
#include "TimerManager.h"
#include "SDController.h"
#include "Alert/AlertState.h"
#include "AudioState.h"
#include "WiFiController.h"
#include <HTTPClient.h>
#include <WiFiClient.h>

extern TimerManager timers;

namespace {

bool pendingPatterns = false;
bool pendingColors   = false;
uint8_t consecutiveFails = 0;          // tracks escalation stage in checkHealth()

constexpr uint32_t PUSH_INTERVAL_MS   = 10000;                   // retry every 10s
constexpr uint32_t HEALTH_INTERVAL_MS = 57UL * 60UL * 1000UL;   // 57 minutes
constexpr uint32_t NAS_TIMEOUT_MS     = 1500;                    // short timeout

// ── Escalating recovery after health-check failure ──────────
// Fail 1-2:  fast retry every FAST_RETRY_MS (2 min)
// Fail 3:    reconnect WiFi (resets TCP/IP stack), retry after POST_RECONNECT_MS (30s)
// Fail 4+:   give up fast path, reset counter, resume HEALTH_INTERVAL_MS (57 min)
// Any success at any stage: reset counter, resume HEALTH_INTERVAL_MS
constexpr uint8_t  FAST_RETRY_MAX     = 3;                       // fast retries before WiFi reconnect
constexpr uint32_t FAST_RETRY_MS      = 2UL * 60UL * 1000UL;    // 2 min between fast retries
constexpr uint32_t POST_RECONNECT_MS  = 30000;                   // 30s after WiFi reconnect

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
// Timer callback — NAS health probe (interval set by checkHealth)
// Normal: every 57 min.  After failure: escalates per recovery strategy.
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

/// Probe NAS health and schedule the next check.
/// Uses one-shot timers (repeat=1) whose interval depends on the outcome:
///   ok          → 57 min  (normal monitoring)
///   fail 1-2    → 2 min   (fast retry)
///   fail 3      → 30s     (after WiFi reconnect)
///   fail 4+     → 57 min  (give up fast path)
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

    if (ok) {
        if (consecutiveFails > 0) {
            PF("[NasBackup] NAS recovered after %u retries\n", consecutiveFails);
        }
        consecutiveFails = 0;
        // Normal cycle: recheck in 57 minutes
        timers.restart(HEALTH_INTERVAL_MS, 1, cb_checkNasHealth);
    } else {
        consecutiveFails++;
        PF("[NasBackup] NAS unreachable (HTTP %d), attempt %u/%u\n",
           httpCode, consecutiveFails, FAST_RETRY_MAX);

        if (consecutiveFails < FAST_RETRY_MAX) {
            // Fast retry in 2 minutes
            timers.restart(FAST_RETRY_MS, 1, cb_checkNasHealth);
        } else if (consecutiveFails == FAST_RETRY_MAX) {
            // TCP/IP stack may be stale — reconnect WiFi and retry
            PL("[NasBackup] Fast retries exhausted — reconnecting WiFi");
            bootWiFiConnect();
            timers.restart(POST_RECONNECT_MS, 1, cb_checkNasHealth);
        } else {
            // Post-reconnect also failed — resume slow cycle
            PL("[NasBackup] Still unreachable after reconnect — resuming slow cycle");
            consecutiveFails = 0;
            timers.restart(HEALTH_INTERVAL_MS, 1, cb_checkNasHealth);
        }
    }
}
