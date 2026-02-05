#pragma once
/**
 * @file LogRoutes.h
 * @brief LogRoutes implementation
 * @version 260202A
 * @date 2026-02-02
 */
#include <ESPAsyncWebServer.h>

namespace LogRoutes {
    void attachRoutes(AsyncWebServer &server);
}