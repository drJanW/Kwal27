/**
 * @file SdHandlers.h
 * @brief SD card API endpoint handlers
 * @version 251231E
 * @date 2025-12-31
 *
 * Header for SD card web API handlers.
 * Declares handlers for SD card status queries and file uploads.
 * Upload handling includes chunked data reception with proper
 * file path validation to prevent directory traversal attacks.
 */

#pragma once

#include <ESPAsyncWebServer.h>

namespace SdHandlers {

void handleStatus(AsyncWebServerRequest *request);
void handleUploadRequest(AsyncWebServerRequest *request);
void handleUploadData(AsyncWebServerRequest *request, const String &filename,
                      size_t index, uint8_t *data, size_t len, bool final);

void attachRoutes(AsyncWebServer &server);

} // namespace SdHandlers
