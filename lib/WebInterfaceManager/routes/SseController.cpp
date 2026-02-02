/**
 * @file SseController.cpp
 * @brief Server-Sent Events (SSE) management
 * @version 251231E
 * @date 2025-12-31
 *
 * Implements the SSE event source for real-time updates to web clients.
 * Uses WebGuiStatus for unified state push via "state", "patterns", "colors" events.
 * Events are broadcast to all connected clients via the /events endpoint.
 * 
 * CRITICAL: onConnect runs in async_tcp context - SSE sends must be deferred
 * to main loop via TimerManager to avoid watchdog timeout.
 */

#include <Arduino.h>
#include "SseController.h"
#include "../WebGuiStatus.h"
#include "Globals.h"
#include "TimerManager.h"

#ifndef WEBIF_LOG_LEVEL
#define WEBIF_LOG_LEVEL 1
#endif

#if WEBIF_LOG_LEVEL
#define WEBIF_LOG(...) PF(__VA_ARGS__)
#else
#define WEBIF_LOG(...) do {} while (0)
#endif

namespace SseController {

namespace {
AsyncEventSource *eventsPtr = nullptr;

/**
 * @brief Callback for deferred SSE push (runs in main loop context)
 * 
 * CRITICAL: This callback is invoked by TimerManager in main loop,
 * NOT in async_tcp context. Safe to call eventsPtr_->send() here.
 */
void cb_deferredPush() {
    WebGuiStatus::pushAll();
}

} // namespace

void setup(AsyncWebServer &server, AsyncEventSource &events)
{
    eventsPtr = &events;

    // Set event source for WebGuiStatus SSE push
    WebGuiStatus::setEventSource(&events);

    // SSE event source for push notifications
    events.onConnect([](AsyncEventSourceClient *client) {
        if (client->lastId()) {
            WEBIF_LOG("[SSE] Client reconnected, lastId=%u\n", client->lastId());
        }
        // DEFER to main loop! Direct send in async_tcp callback causes watchdog crash.
        // Use restart() - multiple clients can connect, reschedule if pending
        timers.restart(10, 1, cb_deferredPush);
    });
    server.addHandler(&events);
}

} // namespace SseController