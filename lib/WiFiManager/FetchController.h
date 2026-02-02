/**
 * @file FetchController.h
 * @brief HTTP fetch for weather/sunrise APIs and NTP time
 * @version 251231E
 * @date 2025-12-31
 *
 * Header file for external data fetching sources.
 * Provides functions to fetch time from NTP servers,
 * retrieve weather data and sunrise/sunset times from
 * external APIs.
 */

#pragma once
#include <Arduino.h>

bool bootFetchController();

namespace FetchController {
    void requestNtpResync();  // Request NTP re-sync (called at midnight)
}
