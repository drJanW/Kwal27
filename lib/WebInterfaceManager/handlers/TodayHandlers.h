/**
 * @file TodayHandlers.h
 * @brief Today API endpoint handlers
 * @version 251231E
 * @date 2025-12-31
 *
 * Header for today-related web API handlers.
 * Declares the handler for retrieving today's data including
 * date, calendar validity, theme box info, and current settings.
 * Used by the web GUI to display current state information.
 */

#pragma once

#include <ESPAsyncWebServer.h>

namespace TodayHandlers {

void handleToday(AsyncWebServerRequest *request);

void attachRoutes(AsyncWebServer &server);

} // namespace TodayHandlers
