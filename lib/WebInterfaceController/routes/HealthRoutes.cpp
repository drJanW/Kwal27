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
#include "ContextController.h"
#include "Calendar/CalendarRun.h"
#include "TodayModels.h"
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

    const auto& ts = ContextController::time();
    if (ts.hasRtcTemperature) {
        json += ",\"rtcTempC\":" + String(ts.rtcTemperatureC, 1);
    }

    TodayState today;
    if (calendarRun.todayRead(today) && today.entry.valid) {
        char dateBuf[6]; // "dd-mm\0"
        snprintf(dateBuf, sizeof(dateBuf), "%02u-%02u",
                 static_cast<unsigned>(today.entry.day),
                 static_cast<unsigned>(today.entry.month));
        json += ",\"calendarDate\":\"" + String(dateBuf) + "\"";
    }

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