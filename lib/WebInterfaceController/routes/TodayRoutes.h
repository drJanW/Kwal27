/**
 * @file TodayRoutes.h
 * @brief Today API endpoint routes
 * @version 260202A
 * @date 2026-02-02
 */
#pragma once

#include <ESPAsyncWebServer.h>

namespace TodayRoutes {

void routeToday(AsyncWebServerRequest *request);

void attachRoutes(AsyncWebServer &server);

} // namespace TodayRoutes