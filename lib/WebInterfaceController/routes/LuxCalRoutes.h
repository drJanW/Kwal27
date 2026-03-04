/**
 * @file LuxCalRoutes.h
 * @brief Lux calibration API endpoint routes
 * @version 260303A
 * @date 2026-03-03
 */
#pragma once

#include <ESPAsyncWebServer.h>

namespace LuxCalRoutes {

void attachRoutes(AsyncWebServer& server);

} // namespace LuxCalRoutes
