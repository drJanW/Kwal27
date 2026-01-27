/**
 * @file OtaHandlers.h
 * @brief OTA update API endpoint handlers
 * @version 251231E
 * @date 2025-12-31
 *
 * Header for OTA (Over-The-Air) update web API handlers.
 * Declares handlers for the arm/confirm/start endpoints that
 * manage the firmware update process. The two-step process
 * prevents accidental updates and allows timeout expiration.
 */

#pragma once

#include <ESPAsyncWebServer.h>

namespace OtaHandlers {

void handleArm(AsyncWebServerRequest *request);
void handleConfirm(AsyncWebServerRequest *request);
void handleStart(AsyncWebServerRequest *request);

void attachRoutes(AsyncWebServer &server);

} // namespace OtaHandlers
