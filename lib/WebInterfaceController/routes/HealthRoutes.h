/**
 * @file HealthRoutes.h
 * @brief Health API endpoint routes
 * @version 260316C
 * @date 2026-03-16
 */
#pragma once

#include <ESPAsyncWebServer.h>

namespace HealthRoutes {

void routeHealth(AsyncWebServerRequest *request);
void attachRoutes(AsyncWebServer &server);

} // namespace HealthRoutes