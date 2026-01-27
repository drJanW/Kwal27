/**
 * @file HealthHandlers.h
 * @brief Health API endpoint handlers
 * @version 251231E
 * @date 2025-12-31
 *
 * Header for health check web API handler.
 * Declares the handler function for the /api/health endpoint
 * that reports firmware version, health bits, and timer statistics.
 * Useful for monitoring device status via the web interface.
 */

#pragma once

#include <ESPAsyncWebServer.h>

namespace HealthHandlers {

void handleHealth(AsyncWebServerRequest *request);
void attachRoutes(AsyncWebServer &server);

} // namespace HealthHandlers
