/**
 * @file FetchController.h
 * @brief HTTP fetch for weather/sunrise APIs and NTP time
 * @version 260202A
 $12026-02-05
 */
#pragma once
#include <Arduino.h>

bool bootFetchController();

namespace FetchController {
    void requestNtpResync();  // Request NTP re-sync (called at midnight)
}
