/**
 * @file SdRoutes.h
 * @brief SD card API endpoint routes
 * @version 251231E
 * @date 2025-12-31
 *
 * Header for SD card web API routes.
 * Declares routes for SD card status queries and file uploads.
 * Upload routing includes chunked data reception with proper
 * file path validation to prevent directory traversal attacks.
 */

#pragma once

#include <ESPAsyncWebServer.h>

namespace SdRoutes {

void routeStatus(AsyncWebServerRequest *request);
void routeFileDownload(AsyncWebServerRequest *request);
void routeUploadRequest(AsyncWebServerRequest *request);
void routeUploadData(AsyncWebServerRequest *request, const String &filename,
                     size_t index, uint8_t *data, size_t len, bool final);

void attachRoutes(AsyncWebServer &server);

} // namespace SdRoutes