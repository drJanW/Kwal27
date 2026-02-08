/**
 * @file AudioRoutes.cpp
 * @brief Audio API endpoint routes
 * @version 260205A
 * @date 2026-02-05
 */
#include <Arduino.h>
#include "AudioRoutes.h"
#include "../WebGuiStatus.h"
#include "../WebUtils.h"
#include "Globals.h"
#include "MathUtils.h"
#include "RunManager.h"
#include "AudioState.h"

#ifndef WEBIF_LOG_LEVEL
#define WEBIF_LOG_LEVEL LOG_BOOT_SPAM
#endif

#if WEBIF_LOG_LEVEL
#define WEBIF_LOG(...) PF(__VA_ARGS__)
#else
#define WEBIF_LOG(...) do {} while (0)
#endif

using WebUtils::sendJson;

namespace AudioRoutes {

void routeSetLevel(AsyncWebServerRequest *request)
{
    if (!request->hasParam("value")) {
        request->send(400, "text/plain", "Missing ?value");
        return;
    }
    String valStr = request->getParam("value")->value();
    int sliderPct = valStr.toInt();
    // No constrain - JS already ensures sliderPct in loPct..hiPct range
    // Using map() - mapRange clamps internally (A5: no safety theatre)
    
    // Map sliderPct to target volume using Globals (like brightness)
    float targetVolume = MathUtils::mapRange(
        sliderPct, Globals::loPct, Globals::hiPct,
        Globals::volumeLo, Globals::volumeHi);
    
    // Calculate webShift: what multiplier on shiftedHi gives targetVolume?
    float shiftedHi = getVolumeShiftedHi();
    float webShift = (shiftedHi > 0.0f) ? (targetVolume / shiftedHi) : 1.0f;
    RunManager::requestSetAudioLevel(webShift);
    
    // Trigger SSE state push (value ignored - reads from getAudioSliderPct)
    WebGuiStatus::setAudioLevel(0.0f);
    
    WEBIF_LOG("[Web] Audio sliderPct=%d → targetVol=%.2f webShift=%.2f shiftedHi=%.2f\n", 
              sliderPct, targetVolume, webShift, shiftedHi);
    
    request->send(200, "text/plain", "OK");
}

void routeGetLevel(AsyncWebServerRequest *request)
{
    request->send(200, "text/plain", String(getAudioSliderPct()));
}

void routeNext(AsyncWebServerRequest *request)
{
    RunManager::requestWebAudioNext(Globals::webAudioNextFadeMs);
    request->send(200, "text/plain", "OK");
    WEBIF_LOG("[Web] Audio next triggered (fade %u ms)\n", Globals::webAudioNextFadeMs);
}

void routeCurrent(AsyncWebServerRequest *request)
{
    if (!isFragmentPlaying()) {
        request->send(200, "text/plain", "-");
        return;
    }
    uint8_t dir = 0, file = 0, score = 0;
    if (getCurrentDirFile(dir, file, score)) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%u/%u", dir, file);
        request->send(200, "text/plain", buf);
    } else {
        request->send(200, "text/plain", "-");
    }
}

void routePlay(AsyncWebServerRequest *request)
{
    if (!request->hasParam("dir")) {
        request->send(400, "text/plain", "Missing ?dir");
        return;
    }
    uint8_t dir = static_cast<uint8_t>(request->getParam("dir")->value().toInt());
    int8_t file = -1;  // Default: random from dir
    if (request->hasParam("file")) {
        file = request->getParam("file")->value().toInt();
    }
    RunManager::requestPlaySpecificFragment(dir, file);
    request->send(200, "text/plain", "OK");
    WEBIF_LOG("[Web] Play %u/%d triggered\n", dir, file);
}

void attachRoutes(AsyncWebServer &server)
{
    server.on("/setWebAudioLevel", HTTP_GET, routeSetLevel);
    server.on("/setWebAudioLevel", HTTP_POST, routeSetLevel);  // Accept both GET and POST
    server.on("/getWebAudioLevel", HTTP_GET, routeGetLevel);
    server.on("/api/audio/next", HTTP_POST, routeNext);
    server.on("/api/audio/current", HTTP_GET, routeCurrent);
    server.on("/api/audio/play", HTTP_GET, routePlay);
}

} // namespace AudioRoutes