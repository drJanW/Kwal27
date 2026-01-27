/**
 * @file NotifyIntent.h
 * @brief Hardware notification intent enumeration
 * @version 251231G
 * @date 2025-12-31
 *
 * Defines the NotifyIntent enum for all hardware status notifications.
 * Each component has an OK/FAIL pair for reporting initialization results.
 * Used by modules to report status to the notification coordinator.
 */

#pragma once

#include <stdint.h>

enum class NotifyIntent : uint8_t {
    // Component status (OK/FAIL pairs)
    SD_OK,
    SD_FAIL,
    WIFI_OK,
    WIFI_FAIL,
    RTC_OK,
    RTC_FAIL,
    NTP_OK,
    NTP_FAIL,
    DISTANCE_SENSOR_OK,
    DISTANCE_SENSOR_FAIL,
    LUX_SENSOR_OK,
    LUX_SENSOR_FAIL,
    SENSOR3_OK,
    SENSOR3_FAIL,
    TTS_OK,
    TTS_FAIL,
    
    // Boot lifecycle
    START_RUNTIME,
};
