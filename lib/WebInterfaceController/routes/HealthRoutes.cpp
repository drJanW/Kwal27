/**
 * @file HealthRoutes.cpp
 * @brief Health API endpoint routes
 * @version 260202A
 * @date 2026-02-02
 */
#include <Arduino.h>
#include "HealthRoutes.h"
#include "Globals.h"
#include "Alert/AlertState.h"
#include "TimerManager.h"
#include <ESP.h>

namespace HealthRoutes {

void routeHealth(AsyncWebServerRequest *request) {
    // Build JSON response
    String json = "{";
    json += "\"firmware\":\"" + String(FIRMWARE_VERSION) + "\",";
    json += "\"health\":" + String(AlertState::getHealthBits()) + ",";
    json += "\"boot\":" + String(AlertState::getBootStatus()) + ",";
    json += "\"absent\":" + String(AlertState::getAbsentBits()) + ",";
    json += "\"timers\":" + String(timers.getActiveCount()) + ",";
    json += "\"maxTimers\":" + String(TimerManager::MAX_TIMERS);
    json += "}";
    
    request->send(200, "application/json", json);
}

void cb_restart() {
    ESP.restart();
}

void routeRestart(AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "Restarting...");
    // Delay restart to allow response to be sent
    timers.create(500, 1, cb_restart);
}

void attachRoutes(AsyncWebServer &server) {
    server.on("/api/health", HTTP_GET, routeHealth);
    server.on("/api/restart", HTTP_POST, routeRestart);
}

} // namespace HealthRoutes