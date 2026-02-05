/**
 * @file SseController.h
 * @brief Server-Sent Events (SSE) management
 * @version 260202A
 * @date 2026-02-02
 */
#pragma once

#include <ESPAsyncWebServer.h>

namespace SseController {

// Setup SSE route
void setup(AsyncWebServer &server, AsyncEventSource &events);

} // namespace SseController