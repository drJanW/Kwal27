/**
 * @file HealthRoutes.cpp
 * @brief Health API endpoint routes
 * @version 260215C
 * @date 2026-02-15
 */
#include <Arduino.h>
#include "HealthRoutes.h"
#include "Globals.h"
#include "Alert/AlertState.h"
#include "Audio/AudioPolicy.h"
#include "TimerManager.h"
#include "ContextController.h"
#include "Calendar/CalendarRun.h"
#include "TodayState.h"
#include <ESP.h>

namespace HealthRoutes {

void routeHealth(AsyncWebServerRequest *request) {
    // Build JSON response
    String json = "{";
    json += "\"device\":\"" + String(Globals::deviceName) + "\",";
    json += "\"firmware\":\"" + String(Globals::firmwareVersion) + "\",";
    json += "\"health\":" + String(AlertState::getHealthBits()) + ",";
    json += "\"boot\":" + String(AlertState::getBootStatus()) + ",";
    json += "\"absent\":" + String(AlertState::getAbsentBits()) + ",";
    json += "\"timers\":" + String(timers.getActiveCount()) + ",";
    json += "\"maxActiveTimers\":" + String(timers.getMaxActiveTimers()) + ",";
    json += "\"maxTimers\":" + String(MAX_TIMERS);

    const auto& ts = ContextController::time();
    if (ts.hasRtcTemperature) {
        json += ",\"rtcTempC\":" + String(ts.rtcTemperatureC, 1);
    }
    if (ts.synced) {
        char dateBuf[11]; // "dd-mm-yyyy\0"
        snprintf(dateBuf, sizeof(dateBuf), "%02u-%02u-%u",
                 ts.day, ts.month, ts.year);
        json += ",\"ntpDate\":\"" + String(dateBuf) + "\"";
        char timeBuf[6]; // "HH:MM\0"
        snprintf(timeBuf, sizeof(timeBuf), "%02u:%02u", ts.hour, ts.minute);
        json += ",\"ntpTime\":\"" + String(timeBuf) + "\"";
    }

    // Heap stats
    json += ",\"heapFree\":" + String(ESP.getFreeHeap() / 1024);
    json += ",\"heapMin\":" + String(ESP.getMinFreeHeap() / 1024);
    json += ",\"heapBlock\":" + String(ESP.getMaxAllocHeap() / 1024);

    TodayState today;
    if (calendarRun.todayRead(today) && today.entry.valid) {
        char dateBuf[6]; // "dd-mm\0"
        snprintf(dateBuf, sizeof(dateBuf), "%02u-%02u",
                 static_cast<unsigned>(today.entry.day),
                 static_cast<unsigned>(today.entry.month));
        json += ",\"calendarDate\":\"" + String(dateBuf) + "\"";
    }

    // Active theme box name
    const String& boxId = AudioPolicy::themeBoxId();
    if (!boxId.isEmpty()) {
        long numId = boxId.toInt();
        if (numId > 0) {
            const ThemeBox* tb = FindThemeBox(static_cast<uint8_t>(numId));
            if (tb) {
                json += ",\"themeBox\":\"" + tb->name + "\"";
            }
        } else {
            json += ",\"themeBox\":\"" + boxId + "\"";
        }
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