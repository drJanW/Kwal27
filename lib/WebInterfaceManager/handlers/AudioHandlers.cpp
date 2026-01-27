/**
 * @file AudioHandlers.cpp
 * @brief Audio API endpoint handlers
 * @version 251231E
 * @date 2025-12-31
 *
 * Implements HTTP handlers for the /api/audio/* endpoints.
 * Provides routes to get/set audio volume level, skip to next track,
 * and retrieve current playback information. Integrates with AudioState
 * and ConductManager for audio control operations.
 */

#include <Arduino.h>
#include "AudioHandlers.h"
#include "../WebGuiStatus.h"
#include "../WebUtils.h"
#include "Globals.h"
#include "MathUtils.h"
#include "ConductManager.h"
#include "AudioState.h"

#ifndef WEBIF_LOG_LEVEL
#define WEBIF_LOG_LEVEL 1
#endif

#if WEBIF_LOG_LEVEL
#define WEBIF_LOG(...) PF(__VA_ARGS__)
#else
#define WEBIF_LOG(...) do {} while (0)
#endif

using WebUtils::sendJson;

namespace AudioHandlers {

void handleSetLevel(AsyncWebServerRequest *request)
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
        static_cast<float>(sliderPct),
        static_cast<float>(Globals::loPct), static_cast<float>(Globals::hiPct),
        Globals::volumeLo, Globals::volumeHi);
    
    // Calculate webShift: what multiplier on shiftedHi gives targetVolume?
    float shiftedHi = getVolumeShiftedHi();
    float webShift = (shiftedHi > 0.0f) ? (targetVolume / shiftedHi) : 1.0f;
    setVolumeWebShift(webShift);
    
    // Trigger SSE state push (value ignored - reads from getAudioSliderPct)
    WebGuiStatus::setAudioLevel(0.0f);
    
    WEBIF_LOG("[Web] Audio sliderPct=%d → targetVol=%.2f webShift=%.2f shiftedHi=%.2f\n", 
              sliderPct, targetVolume, webShift, shiftedHi);
    
    request->send(200, "text/plain", "OK");
}

void handleGetLevel(AsyncWebServerRequest *request)
{
    request->send(200, "text/plain", String(getAudioSliderPct()));
}

void handleNext(AsyncWebServerRequest *request)
{
    ConductManager::intentWebAudioNext(Globals::webAudioNextFadeMs);
    request->send(200, "text/plain", "OK");
    WEBIF_LOG("[Web] Audio next triggered (fade %u ms)\n", Globals::webAudioNextFadeMs);
}

void handleCurrent(AsyncWebServerRequest *request)
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

void handlePlay(AsyncWebServerRequest *request)
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
    ConductManager::intentPlaySpecificFragment(dir, file);
    request->send(200, "text/plain", "OK");
    WEBIF_LOG("[Web] Play %u/%d triggered\n", dir, file);
}

void attachRoutes(AsyncWebServer &server)
{
    server.on("/setWebAudioLevel", HTTP_GET, handleSetLevel);
    server.on("/setWebAudioLevel", HTTP_POST, handleSetLevel);  // Accept both GET and POST
    server.on("/getWebAudioLevel", HTTP_GET, handleGetLevel);
    server.on("/api/audio/next", HTTP_POST, handleNext);
    server.on("/api/audio/current", HTTP_GET, handleCurrent);
    server.on("/api/audio/play", HTTP_GET, handlePlay);
}

} // namespace AudioHandlers
