/**
 * @file WebInterfaceController.cpp
 * @brief Async web server setup, routes index.html and API endpoints
 * @version 260219C
 * @date 2026-02-19
 */
#include <Arduino.h>
#include "WebInterfaceController.h"
#include "WebGuiStatus.h"
#include "WebUtils.h"
#include "FallbackPage.h"
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
#define WEBIF_LOG_LEVEL LOG_BOOT_SPAM
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

const uint8_t faviconIco[] PROGMEM = {
    0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x10, 0x10, 0x00, 0x00, 0x00, 0x00, 0x20, 0x00, 0x94, 0x00,
    0x00, 0x00, 0x16, 0x00, 0x00, 0x00, 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00,
    0x00, 0x0D, 0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x10, 0x08, 0x06,
    0x00, 0x00, 0x00, 0x1F, 0xF3, 0xFF, 0x61, 0x00, 0x00, 0x00, 0x5B, 0x49, 0x44, 0x41, 0x54, 0x78,
    0x9C, 0x63, 0xE4, 0x92, 0x0B, 0xF8, 0xCF, 0x40, 0x01, 0x60, 0xA2, 0x44, 0x33, 0x03, 0x03, 0x03,
    0x03, 0x0B, 0x36, 0xC1, 0xAF, 0x0B, 0x9A, 0xB0, 0x2A, 0xE6, 0x4E, 0xA8, 0x23, 0xEC, 0x02, 0x5C,
    0x9A, 0x71, 0xC9, 0x31, 0x11, 0x52, 0x40, 0xC8, 0x10, 0x26, 0x5C, 0x12, 0xC4, 0x1A, 0x42, 0x71,
    0x20, 0xC2, 0x0D, 0xC0, 0x16, 0x40, 0xB8, 0x00, 0xB2, 0x5A, 0x26, 0x5C, 0x12, 0x24, 0xBB, 0x80,
    0x54, 0x9B, 0xF1, 0x1A, 0xC0, 0x9D, 0x50, 0x07, 0x57, 0x8C, 0x4E, 0x53, 0xE4, 0x82, 0xC1, 0x69,
    0x00, 0xE3, 0x80, 0xE7, 0x46, 0x00, 0x35, 0xE5, 0x19, 0xE1, 0x39, 0x39, 0xD6, 0x20, 0x00, 0x00,
    0x00, 0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82
};

void routeRoot(AsyncWebServerRequest *request)
{
    if (!AlertState::isSdOk()) {
        request->send_P(200, "text/html", FALLBACK_HTML);
        return;
    }
    if (!SD.exists("/index.html")) {
        request->send_P(200, "text/html", FALLBACK_HTML);
        return;
    }
    request->send(SD, "/index.html", "text/html");
}

void routeFavicon(AsyncWebServerRequest *request)
{
    if (sizeof(faviconIco) == 0) {
        request->send(500, "text/plain", "favicon empty");
        WEBIF_LOG("[Web] /favicon.ico 500 (empty)\n");
        return;
    }
    AsyncWebServerResponse *response = request->beginResponse_P(200, "image/x-icon", faviconIco, sizeof(faviconIco));
    response->addHeader("Cache-Control", "public, max-age=31536000, immutable");
    request->send(response);
}

void routeSetBrightness(AsyncWebServerRequest *request)
{
    if (!request->hasParam("value")) {
        request->send(400, "text/plain", "Missing ?value");
        return;
    }
    String valStr = request->getParam("value")->value();
    int sliderPct = valStr.toInt();
    // Slider sends 0-100 freely (grey zones are visual only)
    // map() converts to brightness range, webMultiplier compensates shifts
    
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
        sliderPct, Globals::loPct, Globals::hiPct,
        Globals::brightnessLo, Globals::brightnessHi);
    
    // 4. Calculate webMultiplier: what would shiftedHi be with webMultiplier=1.0?
    uint8_t baseShiftedHi = LightPolicy::calcShiftedHi(lux, calendarShift, 1.0f);
    float webMultiplier = (baseShiftedHi > 0) ? (targetBrightness / baseShiftedHi) : 1.0f;
    setWebMultiplier(webMultiplier);
    
    // 5. Recalculate shiftedHi with new webMultiplier
    uint8_t shiftedHi = LightPolicy::calcShiftedHi(lux, calendarShift, webMultiplier);
    setBrightnessShiftedHi(shiftedHi);
    
    WEBIF_LOG("[Web] sliderPct=%d → webMultiplier=%.2f shiftedHi=%u\n", sliderPct, webMultiplier, shiftedHi);
    
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
    server.on("/favicon.ico", HTTP_GET, routeFavicon);
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
    PL("[WebInterface] Server started");
}

void updateWebInterface()
{
    // AsyncWebServer: not needed. Keep empty for compatibility.
}
