/**
 * @file OtaRoutes.h
 * @brief OTA update API endpoint — HTTP firmware upload
 * @version 260212A
 * @date 2026-02-12
 */
#pragma once

#include <ESPAsyncWebServer.h>

namespace OtaRoutes {

// HTTP OTA: browser-based firmware upload
void routeUploadFirmwareData(AsyncWebServerRequest *request,
                             const String &filename,
                             size_t index, uint8_t *data, size_t len, bool final);
void routeUploadFirmwareRequest(AsyncWebServerRequest *request);

void attachRoutes(AsyncWebServer &server);

} // namespace OtaRoutes