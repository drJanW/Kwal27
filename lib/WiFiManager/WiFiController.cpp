/**
 * @file WiFiController.cpp
 * @brief WiFi station connection with growing retry interval and connection monitoring
 * @version 260129D
 * @date 2026-01-29
 *
 * Non-blocking WiFi station connect sequence using TimerManager with
 * a growing retry interval. A separate connection check timer verifies the link
 * and restarts the connection flow when WiFi drops.
 */

#include <Arduino.h>
#include "WiFiController.h"
#include <WiFi.h>
#include "TimerManager.h"
#include "Alert/AlertState.h"
#include "HWconfig.h"
#include "Globals.h"

static bool stationConfigured = false;  // Guard: only set station config once
static bool loggedStart = false;     // Log connect start once per attempt

static void cb_checkWiFiStatus();
static void cb_checkWiFiConnection();
static void cb_retryConnect();

static void configureStation() {
    if (stationConfigured) return;
    WiFi.mode(WIFI_STA);
    stationConfigured = true;
}

// Status check timer: detects transitions and gates the flow between
// retry status checks and connection monitoring.
//
// Behavior summary:
// 1) If WiFi reports connected AND a valid local IP is present, this is the
//    first stable connection after the retry loop. We then:
//    - clear the one-shot "starting" log guard
//    - mark WiFi OK in AlertState
//    - stop retry + status timers
//    - start the slower connection check timer
//
// 2) If WiFi is not connected but we previously marked it OK, this is a
//    transition to disconnected. We then:
//    - set AlertState back to "not OK" with the initial retry count
//    - emit a single loss log (no timers are started here)
static void cb_checkWiFiStatus() {
    if (WiFi.status() == WL_CONNECTED && WiFi.localIP() != INADDR_NONE) {
        if (!AlertState::isWifiOk()) {
            // First confirmed connection after retry loop
            loggedStart = false;
            AlertState::setStatusOK(SC_WIFI);
            PF("[WiFi] Connected. IP: %s\n", WiFi.localIP().toString().c_str());
            timers.cancel(cb_retryConnect);
            timers.cancel(cb_checkWiFiStatus);
            timers.create(Globals::wifiConnectionCheckIntervalMs, 0, cb_checkWiFiConnection);
        }
        return;
    }
    if (AlertState::isWifiOk()) {
        // AlertState carries the public WiFi status for UI and /api/health,
        // so we flip it back to "not OK" on a confirmed disconnect.
        // Transition to disconnected state
        AlertState::set(SC_WIFI, static_cast<uint8_t>(abs(Globals::wifiRetryCount)));
        PL("[WiFi] Lost connection");
    }
}

// Retry timer: re-issues WiFi.begin() with growing intervals until retries end.
static void cb_retryConnect() {
    if (WiFi.status() == WL_CONNECTED && WiFi.localIP() != INADDR_NONE) return;

    int remaining = timers.getRepeatCount(cb_retryConnect);
    if (remaining != -1) AlertState::set(SC_WIFI, abs(remaining));

    if (!timers.isActive(cb_retryConnect)) {
        PL("[WiFi] Max retries reached — giving up");
        AlertState::setStatusOK(SC_WIFI, false);
        timers.cancel(cb_checkWiFiStatus);
        return;
    }

    WiFi.disconnect(false);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

// Connection check timer: lightweight check after a successful connection.
// On failure, it restarts the full connect sequence.
static void cb_checkWiFiConnection() {
    if (WiFi.status() == WL_CONNECTED && WiFi.localIP() != INADDR_NONE) return;

    PL("[WiFi] Connection check failed — restarting connection");
    timers.cancel(cb_checkWiFiConnection);
    AlertState::set(SC_WIFI, static_cast<uint8_t>(abs(Globals::wifiRetryCount)));
    bootWiFiConnect();
}

// Public entry: arms the status-check + retry timers and kicks off a connection attempt.
void bootWiFiConnect() {
    configureStation();

    if (!loggedStart) {
        PL("[WiFi] Starting connection with growing interval");
        loggedStart = true;
    }

    // Start a fresh connection attempt (STA only)
    WiFi.disconnect(false);
#if defined(USE_STATIC_IP) && USE_STATIC_IP
    {
        IPAddress local_IP, gateway, subnet, dns;
        local_IP.fromString(STATIC_IP_STR);
        gateway.fromString(STATIC_GATEWAY_STR);
        subnet.fromString(STATIC_SUBNET_STR);
        dns.fromString(STATIC_DNS_STR);
        if (!WiFi.config(local_IP, gateway, subnet, dns))
            PL("[WiFi] Static IP config failed — using DHCP");
    }
#endif
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    // Start timers only once per bootWiFiConnect() cycle:
    // - status check runs frequently to detect the first stable connect
    // - retry timer re-issues WiFi.begin() with growing intervals until it succeeds

    if (!timers.isActive(cb_checkWiFiStatus)) {
        timers.create(Globals::wifiStatusCheckIntervalMs, 0, cb_checkWiFiStatus);
    }
    if (!timers.isActive(cb_retryConnect)) {
        timers.create(Globals::wifiRetryStartMs, Globals::wifiRetryCount, cb_retryConnect, Globals::wifiRetryGrowth);
    }
}
