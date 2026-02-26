/**
 * @file AudioRoutes.cpp
 * @brief Audio API endpoint routes
 * @version 260226A
 * @date 2026-02-26
 */
#include <Arduino.h>
#include "AudioRoutes.h"
#include "../WebGuiStatus.h"
#include "../WebUtils.h"
#include "Globals.h"
#include "MathUtils.h"
#include "RunManager.h"
#include "AudioState.h"
#include "SDController.h"
#include "SDSettings.h"
#include "TodayState.h"

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
    // Slider sends 0-100 freely (grey zones are visual only)
    // mapRange converts to volume range, webMultiplier compensates shifts
    
    // Map sliderPct to target volume using Globals (like brightness)
    float targetVolume = MathUtils::mapRange(
        sliderPct, Globals::loPct, Globals::hiPct,
        Globals::volumeLo, Globals::volumeHi);
    
    // Calculate webMultiplier: what multiplier on shiftedHi gives targetVolume?
    float shiftedHi = getVolumeShiftedHi();
    float webMultiplier = (shiftedHi > 0.0f) ? (targetVolume / shiftedHi) : 1.0f;
    RunManager::requestSetAudioLevel(webMultiplier);
    
    // Trigger SSE state push (value ignored - reads from getAudioSliderPct)
    WebGuiStatus::setAudioLevel(0.0f);
    
    WEBIF_LOG("[Web] Audio sliderPct=%d → targetVol=%.2f webMultiplier=%.2f shiftedHi=%.2f\n", 
              sliderPct, targetVolume, webMultiplier, shiftedHi);
    
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
    // Source: grid/replay/dir+ from JS src= param, or "random" for dir-only
    const char* source = "random";
    if (file >= 0) source = "replay";  // default for specific file
    if (request->hasParam("src")) {
        String s = request->getParam("src")->value();
        if (s == "grid" || s == "grid/file" || s == "grid%2Ffile") source = "grid/file";
        else if (s == "replay") source = "replay";
        else if (s == "dir+" || s == "dir%2B") source = "dir+";
    }
    RunManager::requestPlaySpecificFragment(dir, file, source);
    request->send(200, "text/plain", "OK");
}

void routeThemeBox(AsyncWebServerRequest *request)
{
    if (!request->hasParam("dir")) {
        request->send(400, "text/plain", "Missing ?dir");
        return;
    }
    uint8_t dir = static_cast<uint8_t>(request->getParam("dir")->value().toInt());
    RunManager::requestSetSingleDirThemeBox(dir);
    request->send(200, "text/plain", "OK");
}

void routeSetIntervals(AsyncWebServerRequest *request)
{
    uint32_t speakMinMs = 0, speakMaxMs = 0;
    uint32_t fragMinMs = 0, fragMaxMs = 0;
    uint32_t durationMs = Globals::defaultWebExpiryMs;
    bool silence = false;
    bool hasSpeakRange = false;
    bool hasFragRange = false;

    // Slider sends center value in minutes; firmware expands to ±30% range
    if (request->hasParam("speak")) {
        int raw = request->getParam("speak")->value().toInt();
        uint32_t centerMs = MINUTES(static_cast<uint32_t>(clamp(raw, 1, 720)));
        speakMinMs = static_cast<uint32_t>(centerMs * 0.7f);
        speakMaxMs = static_cast<uint32_t>(centerMs * 1.3f);
        hasSpeakRange = true;
    }
    if (request->hasParam("frag")) {
        int raw = request->getParam("frag")->value().toInt();
        uint32_t centerMs = MINUTES(static_cast<uint32_t>(clamp(raw, 2, 720)));
        fragMinMs = static_cast<uint32_t>(centerMs * 0.7f);
        fragMaxMs = static_cast<uint32_t>(centerMs * 1.3f);
        hasFragRange = true;
    }
    if (request->hasParam("dur")) {
        int raw = request->getParam("dur")->value().toInt();
        durationMs = MINUTES(static_cast<uint32_t>(clamp(raw, 5, 780)));
    }

    RunManager::requestSetAudioIntervals(
        speakMinMs, speakMaxMs, hasSpeakRange,
        fragMinMs, fragMaxMs, hasFragRange,
        silence, durationMs);

    request->send(200, "text/plain", "OK");
}

void routeSetSilence(AsyncWebServerRequest *request)
{
    bool active = request->hasParam("active")
                  && request->getParam("active")->value() == "1";
    RunManager::requestSetSilence(active);
    request->send(200, "text/plain", "OK");
}

void attachRoutes(AsyncWebServer &server)
{
    server.on("/setWebAudioLevel", HTTP_GET, routeSetLevel);
    server.on("/setWebAudioLevel", HTTP_POST, routeSetLevel);  // Accept both GET and POST
    server.on("/getWebAudioLevel", HTTP_GET, routeGetLevel);
    server.on("/api/audio/next", HTTP_POST, routeNext);
    server.on("/api/audio/current", HTTP_GET, routeCurrent);
    server.on("/api/audio/play", HTTP_GET, routePlay);
    server.on("/api/audio/themebox", HTTP_GET, routeThemeBox);
    server.on("/api/audio/grid", HTTP_GET, routeGrid);
    server.on("/api/audio/intervals", HTTP_GET,  routeSetIntervals);
    server.on("/api/audio/intervals", HTTP_POST, routeSetIntervals);
    server.on("/api/audio/silence", HTTP_GET,  routeSetSilence);
    server.on("/api/audio/silence", HTTP_POST, routeSetSilence);
}

void routeGrid(AsyncWebServerRequest *request)
{
    const auto& boxes = GetAllThemeBoxes();

    // Reverse map: dir → most-specific box (fewest entries)
    uint8_t dirBoxMap[SD_MAX_DIRS + 1];
    uint16_t dirBoxSize[SD_MAX_DIRS + 1];
    memset(dirBoxMap, 0, sizeof(dirBoxMap));
    memset(dirBoxSize, 0xFF, sizeof(dirBoxSize));

    for (const auto& box : boxes) {
        uint16_t sz = static_cast<uint16_t>(box.entries.size());
        for (uint16_t dir : box.entries) {
            if (dir <= SD_MAX_DIRS && sz < dirBoxSize[dir]) {
                dirBoxMap[dir] = box.id;
                dirBoxSize[dir] = sz;
            }
        }
    }

    uint8_t highest = SDController::getHighestDirNum();

    String json;
    json.reserve(2048);

    json += F("{\"highest\":");
    json += String(highest);

    // Theme boxes with colors
    json += F(",\"boxes\":[");
    for (size_t i = 0; i < boxes.size(); i++) {
        if (i) json += ',';
        json += F("{\"id\":");
        json += String(boxes[i].id);
        json += F(",\"name\":\"");
        json += boxes[i].name;
        json += F("\",\"color\":\"");
        json += boxes[i].color;
        json += F("\"}");
    }
    json += ']';

    // Dirs: existence + fileCount from root_dirs only
    json += F(",\"dirs\":[");
    bool firstDir = true;
    DirEntry de;
    for (uint8_t d = 1; d <= highest; d++) {
        if (!SDController::readDirEntry(d, &de) || de.fileCount == 0) continue;
        if (!firstDir) json += ',';
        firstDir = false;
        json += F("{\"d\":");
        json += String(d);
        json += F(",\"b\":");
        json += String(dirBoxMap[d]);
        json += F(",\"n\":");
        json += String(de.fileCount);
        json += '}';
    }
    json += F("]}");

    request->send(200, "application/json", json);
}

} // namespace AudioRoutes