/**
 * @file LuxCalRoutes.h
 * @brief Lux calibration API endpoint routes
 * @version 260319A
 * @date 2026-03-19
 */
#pragma once

#include <ESPAsyncWebServer.h>

namespace LuxCalRoutes {

void attachRoutes(AsyncWebServer& server);

} // namespace LuxCalRoutes
