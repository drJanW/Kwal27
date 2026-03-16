/**
 * @file WiFiBoot.cpp
 * @brief WiFi connection one-time initialization implementation
 * @version 260308E
 * @date 2026-03-08
 */
#include <Arduino.h>
#include "WiFiBoot.h"
#include "Globals.h"
#include "WiFiController.h"
#include "FetchController.h"
#include "TimerManager.h"
#include "RunManager.h"
#include "Alert/AlertRun.h"
#include "Alert/AlertState.h"
#include "SDController.h"
#include "NasBackup.h"
#include "Boot/BootSequencer.h"
#include "Boot/Cap.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <SD.h>

namespace {
    static bool fetchCreated = false;
    static bool moduleTimerStarted = false;
    static bool modulesReadyAnnounced = false;
    static bool csvFetchStarted = false;
    static bool csvCapGranted = false;

    constexpr const char* csvFiles[] = {
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
        }
    }

    void removeAllNasCsvFiles() {
        for (const char* filename : csvFiles) {
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

    size_t downloadCsvFile(const char* filename) {
        if (!filename || !*filename) {
            return 0;
        }

        if (!AlertState::isSdOk()) {
            return 0;
        }

        if (AlertState::isSdBusy()) {
            return 0;
        }

        if (!ensureNasDirectory()) {
            return 0;
        }
        String url = buildCsvUrl(filename);
        if (url.isEmpty()) {
            removeNasCsvFile(filename);
            return 0;
        }

        HTTPClient http;
        WiFiClient client;
        http.setTimeout(Globals::csvHttpTimeoutMs);
        if (!http.begin(client, url)) {
            removeNasCsvFile(filename);
            return 0;
        }

        int httpCode = http.GET();
        if (httpCode != HTTP_CODE_OK) {
            http.end();
            removeNasCsvFile(filename);
            return 0;
        }

        const String tempPath = buildCsvTempPath(filename);
        const String finalPath = buildCsvPath(filename);
        SDController::deleteFile(tempPath.c_str());
        File file = SDController::openFileWrite(tempPath.c_str());
        if (!file) {
            http.end();
            removeNasCsvFile(filename);
            return 0;
        }

        size_t written = http.writeToStream(&file);
        SDController::closeFile(file);
        http.end();

        if (written == 0) {
            SDController::deleteFile(tempPath.c_str());
            removeNasCsvFile(filename);
            return 0;
        }

        if (!commitCsvTempFile(tempPath, finalPath)) {
            removeNasCsvFile(filename);
            return 0;
        }

        return written;
    }

    void downloadCsvFilesFromLan() {
        uint8_t count = 0;
        size_t totalBytes = 0;
        for (const char* filename : csvFiles) {
            size_t bytes = downloadCsvFile(filename);
            if (bytes > 0) {
                PF("[WiFiBoot] NAS CSV: %s (%uB)\n", filename, static_cast<unsigned>(bytes));
                count++;
                totalBytes += bytes;
            }
        }

        const uint8_t total = sizeof(csvFiles) / sizeof(csvFiles[0]);
        if (count == 0) {
            PF("[WiFiBoot] NAS CSV: 0/%u fetched, using SD\n", total);
        }
    }

    void grantCsvCap() {
        if (csvCapGranted) return;
        csvCapGranted = true;
        BootSequencer::grant(Cap::CSV);
    }

    void cb_csvPatience() {
        // Boot can't wait forever — go with whatever CSVs are on SD
        PL("[WiFiBoot] NAS slow, boot continues");
        grantCsvCap();
    }

    void cb_retryNasProbe() {
        NasBackup::checkHealth();
        if (AlertState::isNasOk()) {
            timers.cancel(cb_retryNasProbe);
            timers.cancel(cb_csvPatience);
            downloadCsvFilesFromLan();
            grantCsvCap();
            NasBackup::startHealthTimer();
        }
    }

    void cb_moduleInit() {
        if (!RunManager::isClockRunning()) {
            return;
        }

        if (!modulesReadyAnnounced) {
            modulesReadyAnnounced = true;
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
            hwStatus |= HW_WIFI;
            AlertRun::report(AlertRequest::WIFI_OK);
            BootSequencer::grant(Cap::WIFI);
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
            if (!csvFetchStarted) {
                csvFetchStarted = true;
                // First probe — if NAS responds immediately, fetch now
                NasBackup::checkHealth();
                if (AlertState::isNasOk()) {
                    downloadCsvFilesFromLan();
                    grantCsvCap();
                    NasBackup::startHealthTimer();
                } else {
                    // NAS slow — patience timer lets boot continue, retry keeps trying
                    timers.create(Globals::csvFetchWaitMs, 1, cb_csvPatience);
                    timers.create(SECONDS(1), 0, cb_retryNasProbe, 2.0);
                }
            }

            if (!fetchCreated) {
                if (bootFetchController()) {
                    timers.cancel(cb_wifiBootCheck);
                    fetchCreated = true;
                    PL_BOOT("[Main] fetch timers started");
                } else {
                    PL("[Main] Fetch timers failed to start");
                }
            }

            if (!moduleTimerStarted) {
                if (timers.create(1000, 0, cb_moduleInit)) {
                    moduleTimerStarted = true;
                    PL_BOOT("[Main] module timer started");
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
    bootWiFiConnect();
    PL_BOOT("[WiFiBoot] connect started");
}
