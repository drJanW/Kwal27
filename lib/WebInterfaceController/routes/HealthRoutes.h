/**
 * @file HealthRoutes.h
 * @brief Health API endpoint routes
 * @version 251231E
 * @date 2025-12-31
 *
 * Header for health check web API route.
 * Declares the route function for the /api/health endpoint
 * that reports firmware version, health bits, and timer statistics.
 * Useful for monitoring device status via the web interface.
 */

#pragma once

#include <ESPAsyncWebServer.h>

namespace HealthRoutes {

void routeHealth(AsyncWebServerRequest *request);
void attachRoutes(AsyncWebServer &server);

} // namespace HealthRoutes