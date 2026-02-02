/**
 * @file WebInterfaceController.cpp
 * @brief Async web server setup, routes index.html and API endpoints
 * @version 251231E
 * @date 2025-12-31
 *
 * Main web interface implementation for the ESP32 Kwal project.
 * Sets up the ESPAsyncWebServer instance, configures all API routes,
 * serves static files from the SD card (index.html, CSS, JS), and
 * delegates endpoint routing to specialized route modules.
 */

#include <Arduino.h>
#include "WebInterfaceController.h"
#include "WebGuiStatus.h"
#include "WebUtils.h"
#include "Globals.h"
#include "MathUtils.h"
#include "LightController.h"
#include "SDController.h"
#include "RunManager.h"
#include "SDVoting.h"
#include "Alert/AlertState.h"
#include "Light/LightRun.h"

// Route modules
#include "routes/PatternsRoutes.h"
#include "routes/ColorsRoutes.h"
#include "routes/AudioRoutes.h"
#include "routes/SdRoutes.h"
#include "routes/OtaRoutes.h"
#include "routes/TodayRoutes.h"
#include "routes/HealthRoutes.h"
#include "routes/LogRoutes.h"
#include "routes/SseController.h"
#include "Light/LightPolicy.h"
#include "SensorController.h"

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

void routeRoot(AsyncWebServerRequest *request)
{
    if (!AlertState::isSdOk()) {
        request->send(503, "text/plain", "OUT OF ORDER - SD card failure");
        return;
    }
    if (!SD.exists("/index.html")) {
        request->send(500, "text/plain", "index.html niet gevonden");
        return;
    }
    request->send(SD, "/index.html", "text/html");
}

void routeSetBrightness(AsyncWebServerRequest *request)
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
    float lux = SensorController::ambientLux();
    
    // 2. Get calendar shift (simplified: use 0 if shifts disabled)
#ifndef DISABLE_SHIFTS
    int8_t calendarShift = 0;  // TODO: get from shiftTable when needed
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

void routeGetBrightness(AsyncWebServerRequest *request)
{
    request->send(200, "text/plain", String(getSliderPct()));
}

} // namespace

void beginWebInterface()
{
    // SSE setup
    SseController::setup(server, events);

    // Core routes
    server.on("/", HTTP_GET, routeRoot);
    server.on("/setBrightness", HTTP_GET, routeSetBrightness);
    server.on("/setBrightness", HTTP_POST, routeSetBrightness);  // Accept both GET and POST
    server.on("/getBrightness", HTTP_GET, routeGetBrightness);

    // Attach route modules
    AudioRoutes::attachRoutes(server);
    PatternsRoutes::attachRoutes(server, events);
    ColorsRoutes::attachRoutes(server, events);
    SdRoutes::attachRoutes(server);
    OtaRoutes::attachRoutes(server);
    TodayRoutes::attachRoutes(server);
    HealthRoutes::attachRoutes(server);
    LogRoutes::attachRoutes(server);

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
