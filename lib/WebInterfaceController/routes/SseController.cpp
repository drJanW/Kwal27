/**
 * @file SseController.cpp
 * @brief Server-Sent Events (SSE) management
 * @version 260202A
 $12026-02-06
 */
#include <Arduino.h>
#include "SseController.h"
#include "../WebGuiStatus.h"
#include "Globals.h"
#include "TimerManager.h"

#ifndef WEBIF_LOG_LEVEL
#define WEBIF_LOG_LEVEL LOG_BOOT_SPAM
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