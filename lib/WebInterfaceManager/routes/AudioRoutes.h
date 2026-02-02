/**
 * @file AudioRoutes.h
 * @brief Audio API endpoint routes
 * @version 251231E
 * @date 2025-12-31
 *
 * Header for audio-related web API routes.
 * Declares functions for audio level control (get/set),
 * track navigation (next), and current playback status queries.
 * All routes follow the AsyncWebServer request/response pattern.
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