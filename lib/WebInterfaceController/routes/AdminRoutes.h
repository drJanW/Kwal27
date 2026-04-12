/**
 * @file AdminRoutes.h
 * @brief Admin settings API endpoint routes (globals.csv editor)
 * @version 260412A
 * @date 2026-04-12
 */
#pragma once

#include <ESPAsyncWebServer.h>

namespace AdminRoutes {

void attachRoutes(AsyncWebServer &server);

} // namespace AdminRoutes
