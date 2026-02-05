/**
 * @file OtaRoutes.h
 * @brief OTA update API endpoint routes
 * @version 260202A
 * @date 2026-02-02
 */
#pragma once

#include <ESPAsyncWebServer.h>

namespace OtaRoutes {

void routeArm(AsyncWebServerRequest *request);
void routeConfirm(AsyncWebServerRequest *request);
void routeStart(AsyncWebServerRequest *request);

void attachRoutes(AsyncWebServer &server);

} // namespace OtaRoutes