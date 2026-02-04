/**
 * @file WiFiBoot.cpp
 * @brief WiFi connection one-time initialization implementation
 * @version 251231G
 * @date 2025-12-31
 *
 * Implements WiFi boot sequence: waits for WiFi connection via WiFiController,
 * triggers NTP fetch via FetchController, monitors clock seeding, and reports
 * START_RUNTIME when all modules are ready.
 */

#include "WiFiBoot.h"
#include "Globals.h"
#include "WiFiController.h"
#include "WiFiPolicy.h"
#include "FetchController.h"
#include "TimerManager.h"
#include "RunManager.h"
#include "Alert/AlertRun.h"
#include "Alert/AlertState.h"
#include "SDController.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <SD.h>

namespace {
    static bool fetchCreated = false;
    static bool moduleTimerStarted = false;
    static bool modulesReadyAnnounced = false;
    static bool csvFetchStarted = false;
    static bool csvFetchCompleted = false;

    constexpr const char* kCsvFiles[] = {
        "globals.csv",
        "calendar.csv",
        "light_patterns.csv",
        "light_colors.csv",
        "theme_boxes.csv",
        "audioShifts.csv",
        "colorsShifts.csv",
        "patternShifts.csv"
    };

    String buildCsvUrl(const char* filename) {
        String base = Globals::csvBaseUrl;
        if (base.isEmpty()) {
            return String();
        }
        if (!base.endsWith("/")) {
            base += "/";
        }
        return base + filename;
    }

    String buildCsvPath(const char* filename) {
        String path = "/nas/";
        path += filename;
        return path;
    }

    String buildCsvTempPath(const char* filename) {
        String path = "/nas/";
        path += filename;
        path += ".tmp";
        return path;
    }
    bool ensureNasDirectory() {
        SDController::lockSD();
        const bool ok = SD.exists("/nas") || SD.mkdir("/nas");
        SDController::unlockSD();
        if (!ok) {
            PL("[WiFiBoot] Failed to create /nas directory");
        }
        return ok;
    }

    void removeNasCsvFile(const char* filename) {
        if (!filename || !*filename) {
            return;
        }
        const String path = buildCsvPath(filename);
        if (SDController::fileExists(path.c_str())) {
            SDController::deleteFile(path.c_str());
            PF("[WiFiBoot] Removed stale NAS CSV: %s\n", path.c_str());
        }
    }

    void removeAllNasCsvFiles() {
        for (const char* filename : kCsvFiles) {
            removeNasCsvFile(filename);
        }
    }

    bool commitCsvTempFile(const String& tempPath, const String& finalPath) {
        SDController::deleteFile(finalPath.c_str());
        SDController::lockSD();
        bool renamed = SD.rename(tempPath.c_str(), finalPath.c_str());
        SDController::unlockSD();
        if (!renamed) {
            SDController::deleteFile(tempPath.c_str());
        }
        return renamed;
    }

    bool downloadCsvFile(const char* filename) {
        if (!filename || !*filename) {
            return false;
        }

        if (!AlertState::isSdOk()) {
            PF("[WiFiBoot] CSV download skipped (SD not ready): %s\n", filename);
            return false;
        }

        if (AlertState::isSdBusy()) {
            PF("[WiFiBoot] CSV download skipped (SD busy): %s\n", filename);
            return false;
        }

        if (!ensureNasDirectory()) {
            PF("[WiFiBoot] CSV download skipped (no /nas): %s\n", filename);
            return false;
        }
        String url = buildCsvUrl(filename);
        if (url.isEmpty()) {
            PF("[WiFiBoot] CSV base URL empty, cannot fetch %s\n", filename);
            removeNasCsvFile(filename);
            return false;
        }

        HTTPClient http;
        WiFiClient client;
        http.setTimeout(Globals::csvHttpTimeoutMs);
        if (!http.begin(client, url)) {
            PF("[WiFiBoot] CSV HTTP begin failed: %s\n", url.c_str());
            removeNasCsvFile(filename);
            return false;
        }

        int httpCode = http.GET();
        if (httpCode != HTTP_CODE_OK) {
            PF("[WiFiBoot] CSV HTTP GET failed: %s (code %d)\n", url.c_str(), httpCode);
            http.end();
            removeNasCsvFile(filename);
            return false;
        }

        const String tempPath = buildCsvTempPath(filename);
        const String finalPath = buildCsvPath(filename);
        SDController::deleteFile(tempPath.c_str());
        File file = SDController::openFileWrite(tempPath.c_str());
        if (!file) {
            PF("[WiFiBoot] CSV open failed: %s\n", tempPath.c_str());
            http.end();
            removeNasCsvFile(filename);
            return false;
        }

        size_t written = http.writeToStream(&file);
        SDController::closeFile(file);
        http.end();

        if (written == 0) {
            PF("[WiFiBoot] CSV download empty: %s\n", filename);
            SDController::deleteFile(tempPath.c_str());
            removeNasCsvFile(filename);
            return false;
        }

        if (!commitCsvTempFile(tempPath, finalPath)) {
            PF("[WiFiBoot] CSV replace failed: %s\n", finalPath.c_str());
            removeNasCsvFile(filename);
            return false;
        }

        PF("[WiFiBoot] CSV downloaded: %s -> %s (%u bytes)\n", filename, finalPath.c_str(), static_cast<unsigned>(written));
        return true;
    }

    void downloadCsvFilesFromLan() {
        bool anyOk = false;
        for (const char* filename : kCsvFiles) {
            if (downloadCsvFile(filename)) {
                anyOk = true;
            }
        }

        if (anyOk) {
            PL("[WiFiBoot] CSV download complete");
        } else {
            PL("[WiFiBoot] CSV download failed, using SD fallback");
        }
    }

    void cb_csvFetchTimeout() {
        if (csvFetchCompleted) {
            return;
        }
        csvFetchCompleted = true;
        PL("[WiFiBoot] CSV wait timeout, using SD fallback");
        removeAllNasCsvFiles();
        RunManager::resumeAfterWiFiBoot();
    }

    void cb_moduleInit() {
        if (!RunManager::isClockRunning()) {
            return;
        }

        if (!modulesReadyAnnounced) {
            modulesReadyAnnounced = true;
            if (RunManager::isClockInFallback()) {
                PL("[Main] Bootstrapping (RTC) ready");
            } else {
                PL("[Main] Bootstrapping (NTP) ready");
            }
            // SD passed (we're post-SDBoot), WiFi up, clock running
            AlertRun::report(AlertRequest::START_RUNTIME);
        }

        timers.cancel(cb_moduleInit);
        moduleTimerStarted = false;
    }

    constexpr uint32_t WIFI_WAIT_LOG_INTERVAL_MS = 5000;

    void cb_wifiBootCheck() {
        static bool lastWiFiState = false;
        static uint32_t lastWaitLogMs = 0;

        bool wifiUp = AlertState::isWifiOk();
        if (wifiUp && !lastWiFiState) {
            PF("[Main] WiFi connected: %s\n", WiFi.localIP().toString().c_str());
            hwStatus |= HW_WIFI;
            AlertRun::report(AlertRequest::WIFI_OK);
        } else if (!wifiUp && lastWiFiState) {
            PL("[Main] WiFi lost, retrying");
            hwStatus &= ~HW_WIFI;
            AlertRun::report(AlertRequest::WIFI_FAIL);
        }

        if (!wifiUp) {
            const uint32_t now = millis();
            if (now - lastWaitLogMs >= WIFI_WAIT_LOG_INTERVAL_MS) {
                PL("[Main] WiFi not connected yet");
                lastWaitLogMs = now;
            }
        }
        else {
            lastWaitLogMs = millis();
        }

        lastWiFiState = wifiUp;

        if (wifiUp) {
            if (!csvFetchCompleted && !csvFetchStarted) {
                csvFetchStarted = true;
                downloadCsvFilesFromLan();
                csvFetchCompleted = true;
                timers.cancel(cb_csvFetchTimeout);
                RunManager::resumeAfterWiFiBoot();
            }

            if (!fetchCreated) {
                if (bootFetchController()) {
                    timers.cancel(cb_wifiBootCheck);
                    fetchCreated = true;
                    PL("[Main] Fetch timers running");
                } else {
                    PL("[Main] Fetch timers failed to start");
                }
            }

            if (!moduleTimerStarted) {
                if (timers.create(1000, 0, cb_moduleInit)) {
                    moduleTimerStarted = true;
                    PL("[Main] Module monitor timer started");
                } else {
                    PL("[Main] Failed to start module timer");
                }
            }
        }
    }
}

void WiFiBoot::plan() {
    if (!timers.create(1000, 0, cb_wifiBootCheck)) {
        PL("[Main] Failed to create WiFi boot check timer");
    }
    if (Globals::csvFetchWaitMs > 0 && !timers.isActive(cb_csvFetchTimeout)) {
        timers.create(Globals::csvFetchWaitMs, 1, cb_csvFetchTimeout);
    }
    bootWiFiConnect();
    PL("[Run][Plan] WiFi connect sequence started");
    WiFiPolicy::configure();
}
