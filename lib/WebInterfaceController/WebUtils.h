/**
 * @file WebUtils.h
 * @brief Shared utilities for web routes
 * @version 251231E
 * @date 2025-12-31
 *
 * Common utility functions used across all web API routes.
 * Provides helper functions for sending JSON responses, error responses,
 * JSON string escaping, RGB color conversion, and ID string formatting.
 * All functions are inline for performance in the embedded environment.
 */

#pragma once

#include <ESPAsyncWebServer.h>
#include <Arduino.h>

// Forward declaration
class AsyncWebServerRequest;

namespace WebUtils {

// Send JSON response with optional extra header
inline void sendJson(AsyncWebServerRequest *request, const String &payload,
                     const char *extraHeader = nullptr, const String &extraValue = String())
{
    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", payload);
    response->addHeader("Cache-Control", "no-store");
    if (extraHeader && extraHeader[0] != '\0') {
        response->addHeader(extraHeader, extraValue);
    }
    request->send(response);
}

// Send error response
inline void sendError(AsyncWebServerRequest *request, int code, const String &message)
{
    AsyncWebServerResponse *response = request->beginResponse(code, "text/plain", message);
    response->addHeader("Cache-Control", "no-store");
    request->send(response);
}

// Append JSON-escaped string to output
inline void appendJsonEscaped(String &out, const char *value)
{
    if (!value) return;
    while (*value) {
        const char c = *value++;
        switch (c) {
        case '\\': out += F("\\\\"); break;
        case '"':  out += F("\\\""); break;
        case '\n': out += F("\\n");  break;
        case '\r': out += F("\\r");  break;
        case '\t': out += F("\\t");  break;
        default:
            if (static_cast<unsigned char>(c) >= 0x20) {
                out += c;
            }
            break;
        }
    }
}

// Convert RGB to hex string
inline String rgbToHex(uint8_t r, uint8_t g, uint8_t b)
{
    char buffer[8];
    snprintf(buffer, sizeof(buffer), "#%02X%02X%02X",
             static_cast<unsigned>(r),
             static_cast<unsigned>(g),
             static_cast<unsigned>(b));
    return String(buffer);
}

// Convert ID to string (empty if 0)
inline String toIdString(uint8_t id)
{
    if (id == 0) return String();
    char buffer[6];
    snprintf(buffer, sizeof(buffer), "%u", static_cast<unsigned>(id));
    return String(buffer);
}

} // namespace WebUtils
