/**
 * @file OtaRoutes.h
 * @brief OTA update API endpoint routes
 * @version 251231E
 * @date 2025-12-31
 *
 * Header for OTA (Over-The-Air) update web API routes.
 * Declares routes for the arm/confirm/start endpoints that
 * manage the firmware update flow. The two-step flow
 * prevents accidental updates and allows timeout expiration.
 */

#pragma once

#include <ESPAsyncWebServer.h>

namespace OtaRoutes {

void routeArm(AsyncWebServerRequest *request);
void routeConfirm(AsyncWebServerRequest *request);
void routeStart(AsyncWebServerRequest *request);

void attachRoutes(AsyncWebServer &server);

} // namespace OtaRoutes