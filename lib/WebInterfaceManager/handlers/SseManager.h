/**
 * @file SseManager.h
 * @brief Server-Sent Events (SSE) management
 * @version 251231E
 * @date 2025-12-31
 *
 * Header for the SSE (Server-Sent Events) manager.
 * Sets up the /api/events endpoint and integrates with WebGuiStatus
 * for pushing state, patterns, and colors events to connected clients.
 */

#pragma once

#include <ESPAsyncWebServer.h>

namespace SseManager {

// Setup SSE handler
void setup(AsyncWebServer &server, AsyncEventSource &events);

} // namespace SseManager
