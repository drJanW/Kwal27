/**
 * @file AudioRoutes.h
 * @brief Audio API endpoint routes
 * @version 260202A
 * @date 2026-02-02
 */
#pragma once

#include <ESPAsyncWebServer.h>

namespace AudioRoutes {

void routeSetLevel(AsyncWebServerRequest *request);
void routeGetLevel(AsyncWebServerRequest *request);
void routeNext(AsyncWebServerRequest *request);
void routeCurrent(AsyncWebServerRequest *request);
void routePlay(AsyncWebServerRequest *request);

void attachRoutes(AsyncWebServer &server);

} // namespace AudioRoutes