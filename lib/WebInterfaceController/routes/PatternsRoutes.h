/**
 * @file PatternsRoutes.h
 * @brief Patterns API endpoint routes
 * @version 260202A
 $12026-02-05
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