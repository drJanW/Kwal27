/**
 * @file AudioHandlers.h
 * @brief Audio API endpoint handlers
 * @version 251231E
 * @date 2025-12-31
 *
 * Header for audio-related web API handlers.
 * Declares functions for handling audio level control (get/set),
 * track navigation (next), and current playback status queries.
 * All handlers follow the AsyncWebServer request/response pattern.
 */

#pragma once

#include <ESPAsyncWebServer.h>

namespace AudioHandlers {

void handleSetLevel(AsyncWebServerRequest *request);
void handleGetLevel(AsyncWebServerRequest *request);
void handleNext(AsyncWebServerRequest *request);
void handleCurrent(AsyncWebServerRequest *request);

void attachRoutes(AsyncWebServer &server);

} // namespace AudioHandlers
