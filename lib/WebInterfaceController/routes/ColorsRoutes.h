/**
 * @file ColorsRoutes.h
 * @brief Colors API endpoint routes
 * @version 260202A
 $12026-02-05
 */
#pragma once

#include <ESPAsyncWebServer.h>

namespace ColorsRoutes {

void routeList(AsyncWebServerRequest *request);
void routeNext(AsyncWebServerRequest *request);
void routePrev(AsyncWebServerRequest *request);

// Attach all color routes to server
void attachRoutes(AsyncWebServer &server, AsyncEventSource &events);

} // namespace ColorsRoutes