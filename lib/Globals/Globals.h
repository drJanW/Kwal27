/**
 * @file Globals.h
 * @brief Global constants, timing intervals, and utility functions
 * @version 260206K
 * @date 2026-02-06
 */
#pragma once

#include <Arduino.h>
#include "MathUtils.h"
#include "macros.inc"
#include "HWconfig.h"
#include <atomic>
#include <type_traits>

// Firmware version - used by /api/health and Serial output
#if KWAL == HOUT
  #define DEVICE_PREFIX "HOUT-"
#else
  #define DEVICE_PREFIX "MARMER-"
#endif
#define FIRMWARE_VERSION DEVICE_PREFIX "260209B"

// === Compile-time constants (NOT overridable) ===
#define SECONDS_TICK 1000
#define SECONDS(x) ((x) * 1000UL)
#define MINUTES(x) ((x) * 60UL * 1000UL)
#define HOURS(x)   ((x) * 60UL * 60UL * 1000UL)

// Maximum directories in a themebox (theme_boxes.csv entries column)
#define MAX_THEME_DIRS 500

// Debug flags
#define SHOW_TIMER_STATUS LOG_BOOT_SPAM  // Set true to see timer usage in serial

// Growing interval cap (TimerManager)
constexpr uint32_t MAX_GROWTH_INTERVAL_MS = MINUTES(1200);   // cap

// ─────────────────────────────────────────────────────────────
// Hardware status register (graceful degradation)
// Set bits during boot for available hardware, check before use
// ─────────────────────────────────────────────────────────────
extern uint16_t hwStatus;

void bootRandomSeed();

// ─────────────────────────────────────────────────────────────
// Globals struct: runtime-overridable parameters
// Call Globals::begin() after SD init to load /config/globals.csv
// C++17 inline static: defaults visible here, CSV may override
// ─────────────────────────────────────────────────────────────
struct Globals {
    // ─────────────────────────────────────────────────────────────
    // AUDIO (12 params)
    // ─────────────────────────────────────────────────────────────
    inline static uint32_t minAudioIntervalMs     = MINUTES(6);   // Min wait between ambient audio
    inline static uint32_t maxAudioIntervalMs     = MINUTES(48);  // Max wait between ambient audio
    inline static uint16_t baseFadeMs             = SECONDS(5);   // Default fade duration
    inline static uint16_t webAudioNextFadeMs     = 957U;         // Fade when web UI skips track
    inline static uint8_t  fragmentStartFraction  = 50U;          // Start position % into file (0-100)
    inline static float    volumeLo               = 0.05f;        // Slider Lo boundary (fraction of max)
    inline static float    volumeHi               = MAX_VOLUME;   // Slider Hi boundary (default = hardware max)
    inline static float    basePlaybackVolume     = 0.6f;         // Default playback volume
    inline static float    minDistanceVolume      = 0.2f;         // Volume floor near sensor
    inline static float    pingVolumeMax          = 1.0f;         // Ping sound max volume
    inline static float    pingVolumeMin          = 0.35f;        // Ping sound min volume
    inline static uint16_t busyRetryMs            = 120U;         // Retryinterval when audio busy

    // ─────────────────────────────────────────────────────────────
    // SPEECH (2 params)
    // ─────────────────────────────────────────────────────────────
    inline static uint32_t minSaytimeIntervalMs   = MINUTES(85);  // Min wait between time announcements
    inline static uint32_t maxSaytimeIntervalMs   = MINUTES(145); // Max wait between time announcements
    inline static uint32_t minTemperatureSpeakIntervalMs = MINUTES(60); // Min wait between temperature announcements
    inline static uint32_t maxTemperatureSpeakIntervalMs = MINUTES(140); // Max wait between temperature announcements

    // ─────────────────────────────────────────────────────────────
    // LIGHT/PATTERN (3 params)
    // ─────────────────────────────────────────────────────────────
    inline static uint16_t lightFallbackIntervalMs = 300U;        // Pattern update interval
    inline static uint32_t shiftCheckIntervalMs    = MINUTES(1);  // Check CSV shifts interval
    inline static float    defaultFadeWidth        = 64.0f;       // LED color fade smoothness

    // ─────────────────────────────────────────────────────────────
    // BRIGHTNESS/LUX (10 params)
    // ─────────────────────────────────────────────────────────────
    inline static uint8_t  minBrightness           = 6U;          // Hardware minimum (never fully off)
    inline static uint8_t  maxBrightness           = 242U;        // Hardware maximum
    inline static uint8_t  brightnessLo            = 70U;         // Operational Lo boundary
    inline static uint8_t  brightnessHi            = 242U;        // Operational Hi boundary
    inline static constexpr int loPct               = 0;           // Slider percentage lower bound
    inline static constexpr int hiPct               = 100;         // Slider percentage upper bound
    inline static float    luxMin                  = 0.0f;        // Lux sensor minimum
    inline static float    luxMax                  = 800.0f;      // Lux sensor maximum
    inline static int8_t   luxShiftLo              = -10;         // Lux shift at luxMin
    inline static int8_t   luxShiftHi              = +10;         // Lux shift at luxMax
    inline static float    luxGamma                = 0.4f;        // Stevens' power law exponent (0.33-0.5)
    inline static int8_t   calendarShiftLo         = -20;         // Calendar shift minimum
    inline static int8_t   calendarShiftHi         = +20;         // Calendar shift maximum
    inline static uint16_t maxMilliamps            = 1200U;       // FastLED power limit

    // ─────────────────────────────────────────────────────────────
    // SENSORS (16 params)
    // ─────────────────────────────────────────────────────────────
    // I2C init retry timing - first probe delay and growth multiplier for exponential backoff
    inline static uint16_t distanceSensorInitDelayMs = 500U;     // VL53L1X: first retry delay (ms)
    inline static float    distanceSensorInitGrowth  = 1.5f;      // VL53L1X: interval multiplier per retry (5000 -> 7500 -> 11250...)
    inline static uint16_t luxSensorInitDelayMs      = 1000U;     // VEML7700: first retry delay (ms)
    inline static float    luxSensorInitGrowth       = 1.5f;      // VEML7700: interval multiplier per retry (1000 -> 1500 -> 2250...)
    inline static uint32_t luxMeasurementDelayMs     = 800UL;     // Delay after lux trigger
    inline static uint32_t luxMeasurementIntervalMs  = MINUTES(2); // Lux polling interval
    inline static uint16_t sensorBaseDefaultMs       = 100U;      // Distance sensor base interval
    inline static uint16_t sensorFastIntervalMs      = 30U;       // Fast interval (motion)
    inline static uint16_t sensorFastDurationMs      = 800U;      // Fast interval duration
    inline static float    sensorFastDeltaMm         = 80.0f;     // Delta to trigger fast interval
    inline static uint16_t distanceNewWindowMs       = 1500U;     // Debounce window for readings
    inline static uint16_t distanceSensorDummyMm     = 9999U;     // Fallback if sensor fails
    inline static float    luxSensorDummyLux         = 0.5f;      // Fallback lux if sensor fails
    inline static float    sensor3DummyTemp          = 25.0f;     // Fallback temperature
    inline static float    distanceMinMm             = 40.0f;     // Valid range minimum
    inline static float    distanceMaxMm             = 3600.0f;   // Valid range maximum

    // ─────────────────────────────────────────────────────────────
    // HEARTBEAT (3 params)
    // ─────────────────────────────────────────────────────────────
    inline static uint16_t heartbeatMinMs          = 90U;         // Fastest heartbeat pulse
    inline static uint16_t heartbeatMaxMs          = SECONDS(2);  // Slowest heartbeat pulse
    inline static uint16_t heartbeatDefaultMs      = 500U;        // Default heartbeat rate

    // ─────────────────────────────────────────────────────────────
    // ALERT (7 params)
    // ─────────────────────────────────────────────────────────────
    // One burst = black(1s) + color(1-2s) + black(1s) ≈ 3-4s total
    // Flash burst: immediate flashes when error detected
    inline static uint32_t flashBurstIntervalMs    = SECONDS(10); // Interval between flash bursts
    inline static uint8_t  flashBurstRepeats       = 2U;          // Repeats after initial (1 = 2 bursts total)
    inline static float    flashBurstGrowth        = 2.0f;        // Multiplier for exponential backoff (1.0 = constant)
    // Long-term reminder: periodic flashes after burst completes (2, 20, 200, 2000... min)
    inline static uint32_t reminderIntervalMs      = MINUTES(2);  // First reminder after burst
    inline static float    reminderIntervalGrowth  = 10.0f;       // Multiplier for growing reminder intervals
    inline static uint16_t flashCriticalMs         = SECONDS(2);  // Critical alert flash duration
    inline static uint16_t flashNormalMs           = SECONDS(1);  // Normal alert flash duration

    // ─────────────────────────────────────────────────────────────
    // BOOT/CLOCK (3 params)
    // ─────────────────────────────────────────────────────────────
    inline static uint32_t clockBootstrapIntervalMs  = 500UL;     // Clock init retry interval
    inline static uint32_t ntpFallbackTimeoutMs      = SECONDS(15); // NTP timeout before RTC fallback
    inline static uint32_t bootPhaseMs               = 500UL;     // Delay between boot phases
    inline static uint32_t rtcTemperatureIntervalMs  = MINUTES(3); // RTC temperature read interval

    // ─────────────────────────────────────────────────────────────
    // WIFI (4 params)
    // ─────────────────────────────────────────────────────────────
    inline static uint32_t wifiStatusCheckIntervalMs = 250UL;     // Connection status interval
    inline static uint32_t wifiConnectionCheckIntervalMs = 5000UL; // Connection check interval
    inline static uint32_t wifiRetryStartMs          = 2000UL;    // Retry start interval
    inline static uint8_t  wifiRetryCount            = 14;        // Retry count
    inline static float    wifiRetryGrowth           = 1.5f;      // Retry interval multiplier per attempt

    // ─────────────────────────────────────────────────────────────
    // NETWORK/FETCH (3 params)
    // ─────────────────────────────────────────────────────────────
    inline static uint32_t weatherBootstrapIntervalMs = SECONDS(5); // Weather boot retry interval
    inline static uint32_t weatherRefreshIntervalMs  = HOURS(1);  // Weather API periodic refresh
    inline static uint32_t sunRefreshIntervalMs      = HOURS(2);  // Sunrise/sunset refresh
    inline static uint32_t calendarRefreshIntervalMs = HOURS(1);  // Calendar CSV refresh

    // ─────────────────────────────────────────────────────────────
    // CSV HTTP (3 params)
    // ─────────────────────────────────────────────────────────────
    inline static char     csvBaseUrl[96]            = "http://192.168.2.23:8081/csv/"; // Base URL for CSV fetch
    inline static uint32_t csvHttpTimeoutMs          = 5000UL;    // HTTP timeout per CSV
    inline static uint32_t csvFetchWaitMs            = 6000UL;    // Wait for WiFi before SD fallback

    // ─────────────────────────────────────────────────────────────
    // LOCATION (2 params)
    // ─────────────────────────────────────────────────────────────
    inline static float    locationLat             = 52.37f;      // Latitude for sun calculations (Amsterdam)
    inline static float    locationLon             = 4.90f;       // Longitude for sun calculations (Amsterdam)

    // ─────────────────────────────────────────────────────────────
    // TIME FALLBACK (4 params) — used if NTP+RTC both fail
    // ─────────────────────────────────────────────────────────────
    inline static uint8_t  fallbackMonth           = 4U;          // Fallback month (April)
    inline static uint8_t  fallbackDay             = 20U;         // Fallback day
    inline static uint8_t  fallbackHour            = 4U;          // Fallback hour (04:00)
    inline static uint16_t fallbackYear            = 2026U;       // Fallback year

    // ─────────────────────────────────────────────────────────────
    // DEBUG (2 params)
    // ─────────────────────────────────────────────────────────────
    inline static uint32_t timerStatusIntervalMs   = MINUTES(30);  // Timer pool status log interval
    inline static uint32_t healthStatusIntervalMs  = SECONDS(300); // Health status log interval

    // ─────────────────────────────────────────────────────────────
    // FADE CURVE — shared sine² curve, computed once at boot
    // Used by audio fade (PlayFragment) and brightness fade (LightRun)
    // ─────────────────────────────────────────────────────────────
    static constexpr uint8_t fadeStepCount = 15;
    static float fadeCurve[fadeStepCount];
    static void fillFadeCurve();

    // Guards applyBrightness() during lux fade: both the 50ms repaint timer
    // and the 26ms fade callbacks call FastLED.setBrightness(). Without this
    // flag the repaint would stomp the fade curve mid-step. Set by LightRun,
    // read by LightController. Lives in Globals for cross-library visibility.
    inline static bool brightnessFading = false;

    // Initialize: load CSV overrides (call after SD init)
    static void begin();
};

// ─────────────────────────────────────────────────────────────
// Thread-safe atomic helpers (setMux/getMux)
// Used for cross-core communication on ESP32 dual-core
// ─────────────────────────────────────────────────────────────
#if __cplusplus >= 201703L
  #define MYKWAL_TRIV_COPY(T) std::is_trivially_copyable_v<T>
#else
  #define MYKWAL_TRIV_COPY(T) std::is_trivially_copyable<T>::value
#endif

template <typename T>
inline void setMux(T value, std::atomic<T>* ptr) {
    static_assert(MYKWAL_TRIV_COPY(T), "T moet trivially copyable zijn");
    ptr->store(value, std::memory_order_relaxed);
}

template <typename T>
inline T getMux(const std::atomic<T>* ptr) {
    static_assert(MYKWAL_TRIV_COPY(T), "T moet trivially copyable zijn");
    return ptr->load(std::memory_order_relaxed);
}

using MathUtils::clamp;
using MathUtils::map;
