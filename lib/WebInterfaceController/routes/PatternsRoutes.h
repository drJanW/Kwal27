/**
 * @file PatternsRoutes.h
 * @brief Patterns API endpoint routes
 * @version 251231E
 * @date 2025-12-31
 *
 * Header for pattern-related web API routes.
 * Declares functions for listing light patterns, navigating between
 * patterns (next/prev), and attaching pattern routes to the web server.
 * Routes send SSE events when the active pattern changes.
 */

#pragma once

#include <ESPAsyncWebServer.h>

namespace PatternsRoutes {

void routeList(AsyncWebServerRequest *request);
void routeNext(AsyncWebServerRequest *request);
void routePrev(AsyncWebServerRequest *request);

// Attach all pattern routes to server
void attachRoutes(AsyncWebServer &server, AsyncEventSource &events);

} // namespace PatternsRoutes