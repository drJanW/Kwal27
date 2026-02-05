/**
 * @file HealthRoutes.h
 * @brief Health API endpoint routes
 * @version 260202A
 * @date 2026-02-02
 */
#pragma once

#include <ESPAsyncWebServer.h>

namespace HealthRoutes {

void routeHealth(AsyncWebServerRequest *request);
void attachRoutes(AsyncWebServer &server);

} // namespace HealthRoutes