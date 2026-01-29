/**
 * @file WiFiBoot.cpp
 * @brief WiFi connection one-time initialization implementation
 * @version 251231G
 * @date 2025-12-31
 *
 * Implements WiFi boot sequence: waits for WiFi connection via WiFiManager,
 * triggers NTP fetch via FetchManager, monitors clock seeding, and reports
 * START_RUNTIME when all modules are ready.
 */

#include "WiFiBoot.h"
#include "Globals.h"
#include "WiFiManager.h"
#include "WiFiPolicy.h"
#include "FetchManager.h"
#include "TimerManager.h"
#include "ConductManager.h"
#include "Notify/NotifyConduct.h"
#include "Notify/NotifyState.h"
#include <WiFi.h>

namespace {
    static bool fetchCreated = false;
    static bool moduleTimerStarted = false;
    static bool modulesReadyAnnounced = false;

    void cb_moduleInit() {
        if (!ConductManager::isClockRunning()) {
            return;
        }

        if (!modulesReadyAnnounced) {
            modulesReadyAnnounced = true;
            if (ConductManager::isClockInFallback()) {
                PL("[Main] Bootstrapping (RTC) ready");
            } else {
                PL("[Main] Bootstrapping (NTP) ready");
            }
            // SD passed (we're post-SDBoot), WiFi up, clock running
            NotifyConduct::report(NotifyIntent::START_RUNTIME);
        }

        timers.cancel(cb_moduleInit);
        moduleTimerStarted = false;
    }

    constexpr uint32_t WIFI_WAIT_LOG_INTERVAL_MS = 5000;

    void cb_wifiBootCheck() {
        static bool lastWiFiState = false;
        static uint32_t lastWaitLogMs = 0;

        bool wifiUp = NotifyState::isWifiOk();
        if (wifiUp && !lastWiFiState) {
            PF("[Main] WiFi connected: %s\n", WiFi.localIP().toString().c_str());
            hwStatus |= HW_WIFI;
            NotifyConduct::report(NotifyIntent::WIFI_OK);
        } else if (!wifiUp && lastWiFiState) {
            PL("[Main] WiFi lost, retrying");
            hwStatus &= ~HW_WIFI;
            NotifyConduct::report(NotifyIntent::WIFI_FAIL);
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
            if (!fetchCreated) {
                if (bootFetchManager()) {
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
    bootWiFiConnect();
    PL("[Conduct][Plan] WiFi connect sequence started");
    WiFiPolicy::configure();
}
