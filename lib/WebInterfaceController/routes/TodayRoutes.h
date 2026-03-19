/**
 * @file TodayRoutes.h
 * @brief Today API endpoint routes
 * @version 260221A
 * @date 2026-02-21
 */
#pragma once

#include <ESPAsyncWebServer.h>

namespace TodayRoutes {

void routeToday(AsyncWebServerRequest *request);

void attachRoutes(AsyncWebServer &server);

} // namespace TodayRoutes