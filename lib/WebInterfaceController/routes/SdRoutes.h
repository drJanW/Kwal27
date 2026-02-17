/**
 * @file SdRoutes.h
 * @brief SD card API endpoint routes
 * @version 260217D
 * @date 2026-02-17
 */
#pragma once

#include <ESPAsyncWebServer.h>

namespace SdRoutes {

void routeStatus(AsyncWebServerRequest *request);
void routeFileDownload(AsyncWebServerRequest *request);
void routeListDir(AsyncWebServerRequest *request);
void routeDelete(AsyncWebServerRequest *request);
void routeUploadRequest(AsyncWebServerRequest *request);
void routeUploadData(AsyncWebServerRequest *request, const String &filename,
                     size_t index, uint8_t *data, size_t len, bool final);

void attachRoutes(AsyncWebServer &server);

} // namespace SdRoutes