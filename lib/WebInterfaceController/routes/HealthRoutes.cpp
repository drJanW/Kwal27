/**
 * @file HealthRoutes.cpp
 * @brief Health API endpoint routes
 * @version 260218J
 * @date 2026-02-18
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

    // WiFi config (for fallback page form pre-fill)
    json += ",\"wifiSsid\":\"" + String(Globals::wifiSsid) + "\"";
    json += ",\"staticIp\":\"" + String(Globals::staticIp) + "\"";
    json += ",\"staticGw\":\"" + String(Globals::staticGateway) + "\"";

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

void routeWifiConfig(AsyncWebServerRequest *request) {
    if (!request->hasParam("pin", true) || !request->hasParam("ssid", true)) {
        request->send(400, "application/json", F("{\"error\":\"Missing pin or ssid\"}"));
        return;
    }
    uint16_t pin = request->getParam("pin", true)->value().toInt();
    if (pin != Globals::wifiConfigPin) {
        PL("[WiFi] Web config rejected: wrong PIN");
        request->send(403, "application/json", F("{\"error\":\"Wrong PIN\"}"));
        return;
    }
    String ssid     = request->getParam("ssid", true)->value();
    String password = request->hasParam("password", true) ? request->getParam("password", true)->value() : "";
    String ip       = request->hasParam("ip", true)       ? request->getParam("ip", true)->value()       : "";
    String gateway  = request->hasParam("gateway", true)  ? request->getParam("gateway", true)->value()  : "";
    String name     = request->hasParam("name", true)     ? request->getParam("name", true)->value()     : "";

    PF("[WiFi] POST params: name='%s' ssid='%s' ip='%s' gw='%s'\n",
       name.c_str(), ssid.c_str(), ip.c_str(), gateway.c_str());

    if (!Globals::updateWifiFromWeb(ssid.c_str(), password.c_str(), ip.c_str(), gateway.c_str(), name.c_str())) {
        request->send(400, "application/json", F("{\"error\":\"Invalid config\"}"));
        return;
    }
    request->send(200, "application/json", F("{\"status\":\"ok\",\"message\":\"WiFi config saved. Restart to apply.\"}"));
}

void attachRoutes(AsyncWebServer &server) {
    server.on("/api/health", HTTP_GET, routeHealth);
    server.on("/api/restart", HTTP_POST, routeRestart);
    server.on("/api/wifi/config", HTTP_POST, routeWifiConfig);
}

} // namespace HealthRoutes