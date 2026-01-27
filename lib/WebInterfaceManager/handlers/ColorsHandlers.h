/**
 * @file ColorsHandlers.h
 * @brief Colors API endpoint handlers
 * @version 251231E
 * @date 2025-12-31
 *
 * Header for color-related web API handlers.
 * Declares functions for listing color schemes, navigating between
 * colors (next/prev), and attaching color routes to the web server.
 * Routes send SSE events when the active color changes.
 */

#pragma once

#include <ESPAsyncWebServer.h>

namespace ColorsHandlers {

void handleList(AsyncWebServerRequest *request);
void handleNext(AsyncWebServerRequest *request);
void handlePrev(AsyncWebServerRequest *request);

// Attach all color routes to server
void attachRoutes(AsyncWebServer &server, AsyncEventSource &events);

} // namespace ColorsHandlers
