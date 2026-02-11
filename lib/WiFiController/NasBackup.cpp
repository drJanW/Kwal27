/**
 * @file NasBackup.cpp
 * @brief Push pattern/color CSVs to NAS csv_server.py after save
 * @version 260210J
 * @date 2026-02-10
 *
 * Flow: requestPush(filename) sets a pending flag and schedules a one-shot
 * timer. The timer callback reads the file from SD and POSTs it to the NAS.
 * This avoids blocking the web response that triggered the save.
 */
#include <Arduino.h>

#include "NasBackup.h"
#include "Globals.h"
#include "TimerManager.h"
#include "SDController.h"
#include "Alert/AlertState.h"
#include <HTTPClient.h>
#include <WiFiClient.h>

extern TimerManager timers;

// ─────────────────────────────────────────────────────────────
// Pending push queue (patterns + colors can both be dirty)
// ─────────────────────────────────────────────────────────────
namespace {
    bool pendingPatterns = false;
    bool pendingColors   = false;
    constexpr uint32_t PUSH_DELAY_MS = 1500;   // deferred push interval
}

// ─────────────────────────────────────────────────────────────
// Build upload URL from csvBaseUrl (strip trailing "/csv/" → append "/api/upload")
// csvBaseUrl = "http://192.168.2.23:8081/csv/"
// upload URL = "http://192.168.2.23:8081/api/upload?file=<name>"
// ─────────────────────────────────────────────────────────────
static String buildUploadUrl(const char* filename) {
    String base(Globals::csvBaseUrl);
    // Strip "/csv/" or "/csv" suffix to get server root
    int csvIdx = base.indexOf("/csv");
    if (csvIdx > 0) {
        base = base.substring(0, csvIdx);
    }
    base += "/api/upload?file=";
    base += filename;
    return base;
}

// ─────────────────────────────────────────────────────────────
// Read file from SD into a String
// ─────────────────────────────────────────────────────────────
static bool readFileFromSD(const char* path, String& content) {
    if (!AlertState::isSdOk()) return false;
    File file = SDController::openFileRead(path);
    if (!file) return false;

    size_t fileSize = file.size();
    if (fileSize == 0 || fileSize > 500000) {
        SDController::closeFile(file);
        return false;
    }

    content.reserve(fileSize);
    while (file.available()) {
        content += (char)file.read();
    }
    SDController::closeFile(file);
    return true;
}

// ─────────────────────────────────────────────────────────────
// POST file content to NAS csv_server.py
// ─────────────────────────────────────────────────────────────
static bool postToNas(const char* filename, const String& content) {
    if (!AlertState::isWifiOk()) {
        PF("[NasBackup] WiFi not connected, skipping push for %s\n", filename);
        return false;
    }

    String url = buildUploadUrl(filename);
    PF("[NasBackup] POST %s (%d bytes)\n", url.c_str(), content.length());

    HTTPClient http;
    WiFiClient client;
    http.begin(client, url);
    http.addHeader("Content-Type", "text/csv");
    http.setTimeout(Globals::csvHttpTimeoutMs);

    int httpCode = http.POST(content);
    String response = http.getString();
    http.end();

    if (httpCode == 200) {
        PF("[NasBackup] %s → %s\n", filename, response.c_str());
        return true;
    }

    PF("[NasBackup] %s failed: HTTP %d %s\n", filename, httpCode, response.c_str());
    return false;
}

// ─────────────────────────────────────────────────────────────
// Timer callback — push all pending files
// ─────────────────────────────────────────────────────────────
static void cb_pushToNas() {
    if (pendingPatterns) {
        pendingPatterns = false;
        String content;
        if (readFileFromSD("/light_patterns.csv", content)) {
            postToNas("light_patterns.csv", content);
        } else {
            PF("[NasBackup] failed to read light_patterns.csv from SD\n");
        }
    }

    if (pendingColors) {
        pendingColors = false;
        String content;
        if (readFileFromSD("/light_colors.csv", content)) {
            postToNas("light_colors.csv", content);
        } else {
            PF("[NasBackup] failed to read light_colors.csv from SD\n");
        }
    }
}

// ─────────────────────────────────────────────────────────────
// Public API — called from LightRun after successful save
// ─────────────────────────────────────────────────────────────
void NasBackup::requestPush(const char* filename) {
    if (strcmp(filename, "light_patterns.csv") == 0) {
        pendingPatterns = true;
    } else if (strcmp(filename, "light_colors.csv") == 0) {
        pendingColors = true;
    } else {
        PF("[NasBackup] unknown file: %s\n", filename);
        return;
    }
    // Restart (not create): coalesces rapid saves into one push
    timers.restart(PUSH_DELAY_MS, 1, cb_pushToNas);
}
