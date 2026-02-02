/**
 * @file TodayRoutes.h
 * @brief Today API endpoint routes
 * @version 251231E
 * @date 2025-12-31
 *
 * Header for today-related web API routes.
 * Declares the route for retrieving today's data including
 * date, calendar validity, theme box info, and current settings.
 * Used by the web GUI to display current state information.
 */

#pragma once

#include <ESPAsyncWebServer.h>

namespace TodayRoutes {

void routeToday(AsyncWebServerRequest *request);

void attachRoutes(AsyncWebServer &server);

} // namespace TodayRoutes