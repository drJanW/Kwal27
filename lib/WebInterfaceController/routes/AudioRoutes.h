/**
 * @file AudioRoutes.h
 * @brief Audio API endpoint routes
 * @version 260226A
 * @date 2026-02-26
 */
#pragma once

#include <ESPAsyncWebServer.h>

namespace AudioRoutes {

void routeSetLevel(AsyncWebServerRequest *request);
void routeGetLevel(AsyncWebServerRequest *request);
void routeNext(AsyncWebServerRequest *request);
void routeCurrent(AsyncWebServerRequest *request);
void routePlay(AsyncWebServerRequest *request);
void routeThemeBox(AsyncWebServerRequest *request);
void routeGrid(AsyncWebServerRequest *request);
void routeSetIntervals(AsyncWebServerRequest *request);
void routeSetSilence(AsyncWebServerRequest *request);

void attachRoutes(AsyncWebServer &server);

} // namespace AudioRoutes