/**
 * @file WiFiManager.cpp
 * @brief WiFi connection management with growing retry interval
 * @version 260129D
 * @date 2026-01-29
 *
 * Non-blocking WiFi station connection with automatic retry using
 * TimerManager growing interval. Monitors connection health and
 * triggers reconnection when connectivity is lost.
 */

#include <Arduino.h>
#include "WiFiManager.h"
#include <WiFi.h>
#include "TimerManager.h"
#include "Notify/NotifyState.h"
#include "HWconfig.h"
#include "Globals.h"

static bool modeConfigured = false;
static bool loggedStart = false;

static void cb_checkWiFiStatus();
static void cb_healthCheck();
static void cb_retryConnect();

static void configureStationMode() {
    if (modeConfigured) return;
    WiFi.mode(WIFI_STA);
    modeConfigured = true;
}

static void cb_checkWiFiStatus() {
    if (WiFi.status() == WL_CONNECTED && WiFi.localIP() != INADDR_NONE) {
        if (!NotifyState::isWifiOk()) {
            loggedStart = false;
            NotifyState::setStatusOK(SC_WIFI);
            PF("[WiFi] Connected. IP: %s\n", WiFi.localIP().toString().c_str());
            timers.cancel(cb_retryConnect);
            timers.cancel(cb_checkWiFiStatus);
            timers.create(Globals::wifiHealthIntervalMs, 0, cb_healthCheck);
        }
        return;
    }
    if (NotifyState::isWifiOk()) {
        NotifyState::set(SC_WIFI, static_cast<uint8_t>(abs(Globals::wifiRetryCount)));
        PL("[WiFi] Lost connection");
    }
}

static void cb_retryConnect() {
    if (WiFi.status() == WL_CONNECTED && WiFi.localIP() != INADDR_NONE) return;

    int remaining = timers.getRepeatCount(cb_retryConnect);
    if (remaining != -1) NotifyState::set(SC_WIFI, abs(remaining));

    if (!timers.isActive(cb_retryConnect)) {
        PL("[WiFi] Max retries reached — giving up");
        NotifyState::setStatusOK(SC_WIFI, false);
        timers.cancel(cb_checkWiFiStatus);
        return;
    }

    WiFi.disconnect(false);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

static void cb_healthCheck() {
    if (WiFi.status() == WL_CONNECTED && WiFi.localIP() != INADDR_NONE) return;

    PL("[WiFi] Health check failed — restarting connection");
    timers.cancel(cb_healthCheck);
    NotifyState::set(SC_WIFI, static_cast<uint8_t>(abs(Globals::wifiRetryCount)));
    bootWiFiConnect();
}

void bootWiFiConnect() {
    configureStationMode();

    if (!loggedStart) {
        PL("[WiFi] Starting connection with growing interval");
        loggedStart = true;
    }

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

    if (!timers.isActive(cb_checkWiFiStatus)) {
        timers.create(Globals::wifiPollIntervalMs, 0, cb_checkWiFiStatus);
    }
    if (!timers.isActive(cb_retryConnect)) {
        timers.create(Globals::wifiRetryStartMs, Globals::wifiRetryCount, cb_retryConnect, Globals::wifiRetryGrowth);
    }
}
