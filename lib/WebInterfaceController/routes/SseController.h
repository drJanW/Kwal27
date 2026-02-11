/**
 * @file SseController.h
 * @brief Server-Sent Events (SSE) management
 * @version 260202A
 $12026-02-05
 */
#pragma once

#include <ESPAsyncWebServer.h>

namespace SseController {

// Setup SSE route
void setup(AsyncWebServer &server, AsyncEventSource &events);

} // namespace SseController