/**
 * @file OtaRoutes.h
 * @brief OTA update API endpoint routes (ArduinoOTA + HTTP upload)
 * @version 260206M
 * @date 2026-02-06
 */
#pragma once

#include <ESPAsyncWebServer.h>

namespace OtaRoutes {

void routeArm(AsyncWebServerRequest *request);
void routeConfirm(AsyncWebServerRequest *request);
void routeStart(AsyncWebServerRequest *request);

// HTTP OTA: browser-based firmware upload
void routeUploadFirmwareData(AsyncWebServerRequest *request,
                             const String &filename,
                             size_t index, uint8_t *data, size_t len, bool final);
void routeUploadFirmwareRequest(AsyncWebServerRequest *request);

void attachRoutes(AsyncWebServer &server);

} // namespace OtaRoutes