/**
 * @file HealthRoutes.h
 * @brief Health API endpoint routes
 * @version 260202A
 $12026-02-05
 */
#pragma once

#include <ESPAsyncWebServer.h>

namespace HealthRoutes {

void routeHealth(AsyncWebServerRequest *request);
void attachRoutes(AsyncWebServer &server);

} // namespace HealthRoutes