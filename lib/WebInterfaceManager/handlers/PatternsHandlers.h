/**
 * @file PatternsHandlers.h
 * @brief Patterns API endpoint handlers
 * @version 251231E
 * @date 2025-12-31
 *
 * Header for pattern-related web API handlers.
 * Declares functions for listing light patterns, navigating between
 * patterns (next/prev), and attaching pattern routes to the web server.
 * Routes send SSE events when the active pattern changes.
 */

#pragma once

#include <ESPAsyncWebServer.h>

namespace PatternsHandlers {

void handleList(AsyncWebServerRequest *request);
void handleNext(AsyncWebServerRequest *request);
void handlePrev(AsyncWebServerRequest *request);

// Attach all pattern routes to server
void attachRoutes(AsyncWebServer &server, AsyncEventSource &events);

} // namespace PatternsHandlers
