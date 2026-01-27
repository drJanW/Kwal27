/**
 * @file WiFiManager.cpp
 * @brief WiFi connection management with AP fallback
 * @version 251231E
 * @date 2025-12-31
 *
 * Implementation of WiFi connection management for ESP32.
 * Handles non-blocking WiFi station connection with automatic
 * retry using growing interval. Monitors connection health
 * and triggers reconnection when connectivity is lost.
 * Uses TimerManager for all timing operations to maintain
 * cooperative multitasking behavior.
 */

#include <Arduino.h>
#include "WiFiManager.h"
#include <WiFi.h>
#include "TimerManager.h"
#include "Notify/NotifyState.h"
#include "HWconfig.h"
#include "Globals.h"

// Module state (outside anonymous namespace for external access via isWiFiConnected)
static bool connected = false;

namespace
{
    bool modeConfigured = false;
    bool loggedStart = false;

    constexpr uint32_t pollIntervalMs = 250;
    constexpr uint32_t healthIntervalMs = 5000;
    constexpr uint32_t retryStartMs = 2000;
    constexpr int32_t  retryCount = -14;  // 14 retries with growing interval

    TimerManager& timers()
    {
        return TimerManager::instance();
    }

    void cb_checkWiFiStatus();
    void cb_healthCheck();
    void cb_retryConnect();

    void configureStationMode()
    {
        if (modeConfigured)
            return;
        WiFi.mode(WIFI_STA);
        modeConfigured = true;
    }

    void beginConnect()
    {
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

        // Start poll timer (continuous)
        if (!timers().isActive(cb_checkWiFiStatus)) {
            timers().create(pollIntervalMs, 0, cb_checkWiFiStatus);
        }

        // Arm retry timer with growing interval
        if (!timers().isActive(cb_retryConnect)) {
            timers().create(retryStartMs, retryCount, cb_retryConnect);
        }
    }

    void onConnected()
    {
        connected = true;
        loggedStart = false;
        NotifyState::setStatusOK(SC_WIFI);

        PF("[WiFi] Connected. IP: %s\n", WiFi.localIP().toString().c_str());

        // Stop retry and poll timers, start health check
        timers().cancel(cb_retryConnect);
        timers().cancel(cb_checkWiFiStatus);
        timers().create(healthIntervalMs, 0, cb_healthCheck);
    }

    void onDisconnected()
    {
        if (!connected)
            return;

        connected = false;
        PL("[WiFi] Lost connection");
    }

    void cb_checkWiFiStatus()
    {
        if (WiFi.status() == WL_CONNECTED && WiFi.localIP() != INADDR_NONE)
        {
            if (!connected)
                onConnected();
            return;
        }

        if (connected)
            onDisconnected();
    }

    void cb_retryConnect()
    {
        if (connected)
            return;

        // Update boot status with remaining retries
        int remaining = timers().getRepeatCount(cb_retryConnect);
        if (remaining != -1)
            NotifyState::set(SC_WIFI, abs(remaining));

        // Timer exhausted?
        if (!timers().isActive(cb_retryConnect)) {
            PL("[WiFi] Max retries reached — giving up");
            NotifyState::setStatusOK(SC_WIFI, false);
            timers().cancel(cb_checkWiFiStatus);
            return;
        }

        // Retry
        WiFi.disconnect(false);
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    }

    void cb_healthCheck()
    {
        if (WiFi.status() == WL_CONNECTED && WiFi.localIP() != INADDR_NONE)
            return;

        PL("[WiFi] Health check failed \u2014 restarting connection");
        timers().cancel(cb_healthCheck);
        connected = false;
        beginConnect();
    }
}

void bootWiFiConnect()
{
    beginConnect();
}

bool isWiFiConnected()
{
    return connected;
}
