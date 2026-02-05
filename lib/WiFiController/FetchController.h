/**
 * @file FetchController.h
 * @brief HTTP fetch for weather/sunrise APIs and NTP time
 * @version 260202A
 * @date 2026-02-02
 */
#pragma once
#include <Arduino.h>

bool bootFetchController();

namespace FetchController {
    void requestNtpResync();  // Request NTP re-sync (called at midnight)
}
