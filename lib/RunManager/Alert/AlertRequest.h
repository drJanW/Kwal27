/**
 * @file AlertRequest.h
 * @brief Hardware alert request enumeration
 * @version 260131A
 * @date 2026-01-31
 */
#pragma once

#include <stdint.h>

enum class AlertRequest : uint8_t {
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