/**
 * @file WebInterfaceManager.cpp
 * @brief Async web server setup, routes index.html and API endpoints
 * @version 251231E
 * @date 2025-12-31
 *
 * Main web interface implementation for the ESP32 Kwal project.
 * Sets up the ESPAsyncWebServer instance, configures all API routes,
 * serves static files from the SD card (index.html, CSS, JS), and
 * delegates endpoint handling to specialized handler modules.
 */

#include "WebInterfaceManager.h"
#include "WebGuiStatus.h"
#include "WebUtils.h"
#include "Globals.h"
#include "MathUtils.h"
#include "LightManager.h"
#include "SDManager.h"
#include "ConductManager.h"
#include "SDVoting.h"
#include "Notify/NotifyState.h"
#include "Light/LightConduct.h"

// Handler modules
#include "handlers/PatternsHandlers.h"
#include "handlers/ColorsHandlers.h"
#include "handlers/AudioHandlers.h"
#include "handlers/SdHandlers.h"
#include "handlers/OtaHandlers.h"
#include "handlers/ContextHandlers.h"
#include "handlers/HealthHandlers.h"
#include "handlers/LogHandlers.h"
#include "handlers/SseManager.h"
#include "Light/LightPolicy.h"
#include "SensorManager.h"

#include <ESPAsyncWebServer.h>
#include <SD.h>
#include <WiFi.h>

#ifndef WEBIF_LOG_LEVEL
#define WEBIF_LOG_LEVEL 1
#endif

#if WEBIF_LOG_LEVEL
#define WEBIF_LOG(...) PF(__VA_ARGS__)
#else
#define WEBIF_LOG(...) do {} while (0)
#endif

static AsyncWebServer server(80);
static AsyncEventSource events("/api/events");

using WebUtils::sendJson;

namespace {

void handleRoot(AsyncWebServerRequest *request)
{
    if (!NotifyState::isSdOk()) {
        request->send(503, "text/plain", "OUT OF ORDER - SD card failure");
        return;
    }
    if (!SD.exists("/index.html")) {
        request->send(500, "text/plain", "index.html niet gevonden");
        return;
    }
    request->send(SD, "/index.html", "text/html");
}

void handleSetBrightness(AsyncWebServerRequest *request)
{
    if (!request->hasParam("value")) {
        request->send(400, "text/plain", "Missing ?value");
        return;
    }
    String valStr = request->getParam("value")->value();
    int sliderPct = valStr.toInt();
    // No constrain/clamp here: JS already ensures sliderPct in loPct..hiPct range
    // Using map() not mapRange() - no double clamp needed (A5: no safety theatre)
    
    // 1. Get cached lux (no new measurement)
    float lux = SensorManager::ambientLux();
    
    // 2. Get calendar shift (simplified: use 0 if shifts disabled)
#ifndef DISABLE_SHIFTS
    int8_t calendarShift = 0;  // TODO: get from shiftStore when needed
#else
    int8_t calendarShift = 0;
#endif
    
    // 3. Calculate target brightness from slider percentage
    float targetBrightness = MathUtils::map(
        static_cast<float>(sliderPct),
        static_cast<float>(Globals::loPct), static_cast<float>(Globals::hiPct),
        static_cast<float>(Globals::brightnessLo), static_cast<float>(Globals::brightnessHi));
    
    // 4. Calculate webShift: what would shiftedHi be with webShift=1.0?
    uint8_t baseShiftedHi = LightPolicy::calcShiftedHi(lux, calendarShift, 1.0f);
    float webShift = (baseShiftedHi > 0) ? (targetBrightness / static_cast<float>(baseShiftedHi)) : 1.0f;
    setWebShift(webShift);
    
    // 5. Recalculate shiftedHi with new webShift
    uint8_t shiftedHi = LightPolicy::calcShiftedHi(lux, calendarShift, webShift);
    setBrightnessShiftedHi(shiftedHi);
    
    WEBIF_LOG("[Web] sliderPct=%d → webShift=%.2f shiftedHi=%u\n", sliderPct, webShift, shiftedHi);
    
    request->send(200, "text/plain", "OK");
}

void handleGetBrightness(AsyncWebServerRequest *request)
{
    request->send(200, "text/plain", String(getSliderPct()));
}

} // namespace

void beginWebInterface()
{
    // SSE setup
    SseManager::setup(server, events);

    // Core routes
    server.on("/", HTTP_GET, handleRoot);
    server.on("/setBrightness", HTTP_GET, handleSetBrightness);
    server.on("/setBrightness", HTTP_POST, handleSetBrightness);  // Accept both GET and POST
    server.on("/getBrightness", HTTP_GET, handleGetBrightness);

    // Attach handler modules
    AudioHandlers::attachRoutes(server);
    PatternsHandlers::attachRoutes(server, events);
    ColorsHandlers::attachRoutes(server, events);
    SdHandlers::attachRoutes(server);
    OtaHandlers::attachRoutes(server);
    ContextHandlers::attachRoutes(server);
    HealthHandlers::attachRoutes(server);
    LogHandlers::attachRoutes(server);

    // Serve UI assets from SD card
    server.serveStatic("/styles.css", SD, "/styles.css");
    server.serveStatic("/kwal.js", SD, "/kwal.js");

    // Voting routes
    SDVoting::attachVoteRoute(server);

    server.begin();
    PF("[WebInterface] Ready at http://%s/\n", WiFi.localIP().toString().c_str());
}

void updateWebInterface()
{
    // AsyncWebServer: niet nodig. Leeg laten voor compatibiliteit.
}
