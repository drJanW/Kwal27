/**
 * @file Cap.h
 * @brief Boot capability bitmask — single source of truth for system state
 * @version 260304F
 * @date 2026-03-04
 *
 * Each bit represents a capability the system may or may not have after boot.
 * Steps in the boot manifest declare which capabilities they require (deps)
 * and which they provide (output). The BootSequencer evaluates the manifest
 * and starts steps as their deps become available.
 */
#pragma once
#include <stdint.h>

namespace Cap {
    constexpr uint16_t I2C      = 1 << 0;   ///< I2C bus available
    constexpr uint16_t RTC      = 1 << 1;   ///< RTC hardware read successfully
    constexpr uint16_t SD       = 1 << 2;   ///< SD card mounted + index valid
    constexpr uint16_t CONFIG   = 1 << 3;   ///< globals.csv loaded (or NVS fallback)
    constexpr uint16_t WIFI     = 1 << 4;   ///< WiFi connected
    constexpr uint16_t WEB      = 1 << 5;   ///< Web server running
    constexpr uint16_t NTP      = 1 << 6;   ///< NTP time received
    constexpr uint16_t CLOCK    = 1 << 7;   ///< Time available (NTP, RTC, or fallback)
    constexpr uint16_t CSV      = 1 << 8;   ///< NAS CSV fetch done (or timed out)
    constexpr uint16_t SENSORS  = 1 << 9;   ///< Sensors initialized
    constexpr uint16_t SPEAK    = 1 << 10;  ///< TTS engine configured
    constexpr uint16_t LIGHT_HW = 1 << 11;  ///< FastLED + LED map ready
    constexpr uint16_t AUDIO_HW = 1 << 12;  ///< I2S audio hardware ready
    constexpr uint16_t CALENDAR = 1 << 13;  ///< Calendar loaded for today
    constexpr uint16_t ALL      = 0x3FFF;   ///< All 14 capabilities
}
