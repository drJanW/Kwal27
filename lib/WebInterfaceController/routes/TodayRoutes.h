/**
 * @file TodayRoutes.h
 * @brief Today API endpoint routes
 * @version 260202A
 $12026-02-05
 */
#pragma once

#include <ESPAsyncWebServer.h>

namespace TodayRoutes {

void routeToday(AsyncWebServerRequest *request);

void attachRoutes(AsyncWebServer &server);

} // namespace TodayRoutes