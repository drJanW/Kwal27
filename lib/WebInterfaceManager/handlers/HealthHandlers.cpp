/**
 * @file HealthHandlers.cpp
 * @brief Health API endpoint handlers
 * @version 251231E
 * @date 2025-12-31
 *
 * Implements HTTP handlers for the /api/health endpoint.
 * Returns system health information including firmware version,
 * health status bits from NotifyState, and active timer count.
 * Also provides /api/restart for remote device restart.
 */

#include <Arduino.h>
#include "HealthHandlers.h"
#include "Globals.h"
#include "Notify/NotifyState.h"
#include "TimerManager.h"
#include <ESP.h>

namespace HealthHandlers {

void handleHealth(AsyncWebServerRequest *request) {
    // Build JSON response
    String json = "{";
    json += "\"firmware\":\"" + String(FIRMWARE_VERSION) + "\",";
    json += "\"health\":" + String(NotifyState::getHealthBits()) + ",";
    json += "\"boot\":" + String(NotifyState::getBootStatus()) + ",";
    json += "\"absent\":" + String(NotifyState::getAbsentBits()) + ",";
    json += "\"timers\":" + String(timers.getActiveCount()) + ",";
    json += "\"maxTimers\":" + String(TimerManager::MAX_TIMERS);
    json += "}";
    
    request->send(200, "application/json", json);
}

void cb_restart() {
    ESP.restart();
}

void handleRestart(AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "Restarting...");
    // Delay restart to allow response to be sent
    timers.create(500, 1, cb_restart);
}

void attachRoutes(AsyncWebServer &server) {
    server.on("/api/health", HTTP_GET, handleHealth);
    server.on("/api/restart", HTTP_POST, handleRestart);
}

} // namespace HealthHandlers
