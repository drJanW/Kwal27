/**
 * @file Globals.cpp
 * @brief CSV override loader for Globals
 * @version 251231E
 * @date 2025-12-31
 *
 * Globals::begin() loads /config/globals.csv and overrides known keys.
 * Default values are defined in Globals.h using C++17 inline static.
 * 
 * OVERRIDE MODEL: Code defines truth. CSV may deviate.
 * Missing/corrupt CSV → system runs on code defaults.
 */

#include "Arduino.h"
#include "Globals.h"
#include "SDManager.h"
#include "Alert/AlertState.h"
#include <esp_system.h>

// Hardware status register (graceful degradation)
uint16_t hwStatus = 0;

// ─────────────────────────────────────────────────────────────
// CSV Parser helpers (C-style, no Arduino String, heap-safe)
// ─────────────────────────────────────────────────────────────

static constexpr const char* CSV_PATH = "/globals.csv";
static constexpr size_t MAX_LINE_LEN = 128;

// Find nth semicolon, return pointer after it (or nullptr)
static const char* findField(const char* line, int fieldIndex) {
    const char* p = line;
    for (int i = 0; i < fieldIndex; i++) {
        p = strchr(p, ';');
        if (!p) return nullptr;
        p++;  // skip semicolon
    }
    return p;
}

// Copy field into buffer (up to next ';' or end), return length
static size_t extractField(const char* start, char* buf, size_t bufSize) {
    if (!start || bufSize == 0) return 0;
    size_t i = 0;
    while (start[i] && start[i] != ';' && start[i] != '\r' && start[i] != '\n' && i < bufSize - 1) {
        buf[i] = start[i];
        i++;
    }
    buf[i] = '\0';
    return i;
}

// Parse uint32 from string, return true on success
static bool parseUint32(const char* s, uint32_t* out) {
    if (!s || !*s) return false;
    char* end;
    unsigned long val = strtoul(s, &end, 10);
    if (end == s || *end != '\0') return false;
    *out = static_cast<uint32_t>(val);
    return true;
}

// Parse int32 from string
static bool parseInt32(const char* s, int32_t* out) {
    if (!s || !*s) return false;
    char* end;
    long val = strtol(s, &end, 10);
    if (end == s || *end != '\0') return false;
    *out = static_cast<int32_t>(val);
    return true;
}

// Parse float from string
static bool parseFloat(const char* s, float* out) {
    if (!s || !*s) return false;
    char* end;
    float val = strtof(s, &end);
    if (end == s || *end != '\0') return false;
    *out = val;
    return true;
}

// Parse bool from string (0/1 or true/false)
static bool parseBool(const char* s, bool* out) {
    if (!s || !*s) return false;
    if (strcmp(s, "1") == 0 || strcasecmp(s, "true") == 0) {
        *out = true;
        return true;
    }
    if (strcmp(s, "0") == 0 || strcasecmp(s, "false") == 0) {
        *out = false;
        return true;
    }
    return false;
}

// ─────────────────────────────────────────────────────────────
// Override dispatcher: match key → apply value
// ─────────────────────────────────────────────────────────────

static void applyOverride(const char* key, char type, const char* value) {
    uint32_t u32;
    float f32;
    
    // ═══════════════════════════════════════════════════════════
    // AUDIO
    // ═══════════════════════════════════════════════════════════
    if (strcmp(key, "minAudioIntervalMs") == 0 && type == 'u') {
        if (parseUint32(value, &u32)) {
            Globals::minAudioIntervalMs = u32;
            Serial.printf("[Globals] minAudioIntervalMs = %lu\n", (unsigned long)u32);
        }
    }
    else if (strcmp(key, "maxAudioIntervalMs") == 0 && type == 'u') {
        if (parseUint32(value, &u32)) {
            Globals::maxAudioIntervalMs = u32;
            Serial.printf("[Globals] maxAudioIntervalMs = %lu\n", (unsigned long)u32);
        }
    }
    else if (strcmp(key, "baseFadeMs") == 0 && type == 'u') {
        if (parseUint32(value, &u32) && u32 <= 65535) {
            Globals::baseFadeMs = static_cast<uint16_t>(u32);
            Serial.printf("[Globals] baseFadeMs = %u\n", Globals::baseFadeMs);
        }
    }
    else if (strcmp(key, "webAudioNextFadeMs") == 0 && type == 'u') {
        if (parseUint32(value, &u32) && u32 <= 65535) {
            Globals::webAudioNextFadeMs = static_cast<uint16_t>(u32);
            Serial.printf("[Globals] webAudioNextFadeMs = %u\n", Globals::webAudioNextFadeMs);
        }
    }
    else if (strcmp(key, "fragmentStartFraction") == 0 && type == 'u') {
        if (parseUint32(value, &u32) && u32 <= 100) {
            Globals::fragmentStartFraction = static_cast<uint8_t>(u32);
            Serial.printf("[Globals] fragmentStartFraction = %u\n", Globals::fragmentStartFraction);
        }
    }
    else if (strcmp(key, "volumeLo") == 0 && type == 'f') {
        if (parseFloat(value, &f32)) {
            Globals::volumeLo = f32;
            Serial.printf("[Globals] volumeLo = %.3f\n", f32);
        }
    }
    else if (strcmp(key, "basePlaybackVolume") == 0 && type == 'f') {
        if (parseFloat(value, &f32)) {
            Globals::basePlaybackVolume = f32;
            Serial.printf("[Globals] basePlaybackVolume = %.3f\n", f32);
        }
    }
    else if (strcmp(key, "minDistanceVolume") == 0 && type == 'f') {
        if (parseFloat(value, &f32)) {
            Globals::minDistanceVolume = f32;
            Serial.printf("[Globals] minDistanceVolume = %.3f\n", f32);
        }
    }
    else if (strcmp(key, "pingVolumeMax") == 0 && type == 'f') {
        if (parseFloat(value, &f32)) {
            Globals::pingVolumeMax = f32;
            Serial.printf("[Globals] pingVolumeMax = %.3f\n", f32);
        }
    }
    else if (strcmp(key, "pingVolumeMin") == 0 && type == 'f') {
        if (parseFloat(value, &f32)) {
            Globals::pingVolumeMin = f32;
            Serial.printf("[Globals] pingVolumeMin = %.3f\n", f32);
        }
    }
    else if (strcmp(key, "busyRetryMs") == 0 && type == 'u') {
        if (parseUint32(value, &u32) && u32 <= 65535) {
            Globals::busyRetryMs = static_cast<uint16_t>(u32);
            Serial.printf("[Globals] busyRetryMs = %u\n", Globals::busyRetryMs);
        }
    }
    // ═══════════════════════════════════════════════════════════
    // SPEECH
    // ═══════════════════════════════════════════════════════════
    else if (strcmp(key, "minSaytimeIntervalMs") == 0 && type == 'u') {
        if (parseUint32(value, &u32)) {
            Globals::minSaytimeIntervalMs = u32;
            Serial.printf("[Globals] minSaytimeIntervalMs = %lu\n", (unsigned long)u32);
        }
    }
    else if (strcmp(key, "maxSaytimeIntervalMs") == 0 && type == 'u') {
        if (parseUint32(value, &u32)) {
            Globals::maxSaytimeIntervalMs = u32;
            Serial.printf("[Globals] maxSaytimeIntervalMs = %lu\n", (unsigned long)u32);
        }
    }
    // ═══════════════════════════════════════════════════════════
    // LIGHT/PATTERN
    // ═══════════════════════════════════════════════════════════
    else if (strcmp(key, "lightFallbackIntervalMs") == 0 && type == 'u') {
        if (parseUint32(value, &u32) && u32 <= 65535) {
            Globals::lightFallbackIntervalMs = static_cast<uint16_t>(u32);
            Serial.printf("[Globals] lightFallbackIntervalMs = %u\n", Globals::lightFallbackIntervalMs);
        }
    }
    else if (strcmp(key, "shiftCheckIntervalMs") == 0 && type == 'u') {
        if (parseUint32(value, &u32)) {
            Globals::shiftCheckIntervalMs = u32;
            Serial.printf("[Globals] shiftCheckIntervalMs = %lu\n", (unsigned long)u32);
        }
    }
    else if (strcmp(key, "defaultFadeWidth") == 0 && type == 'f') {
        if (parseFloat(value, &f32)) {
            Globals::defaultFadeWidth = f32;
            Serial.printf("[Globals] defaultFadeWidth = %.1f\n", f32);
        }
    }
    else if (strcmp(key, "maxBrightness") == 0 && type == 'u') {
        if (parseUint32(value, &u32) && u32 <= 255) {
            Globals::maxBrightness = static_cast<uint8_t>(u32);
            Serial.printf("[Globals] maxBrightness = %u\n", Globals::maxBrightness);
        }
    }
    // ═══════════════════════════════════════════════════════════
    // BRIGHTNESS/LUX
    // ═══════════════════════════════════════════════════════════
    // brightnessFloor REMOVED - use brightnessLo instead
    // luxBeta REMOVED - replaced by luxShiftLo/luxShiftHi
    else if (strcmp(key, "luxMin") == 0 && type == 'f') {
        if (parseFloat(value, &f32)) {
            Globals::luxMin = f32;
            Serial.printf("[Globals] luxMin = %.1f\n", f32);
        }
    }
    else if (strcmp(key, "luxMax") == 0 && type == 'f') {
        if (parseFloat(value, &f32)) {
            Globals::luxMax = f32;
            Serial.printf("[Globals] luxMax = %.1f\n", f32);
        }
    }
    else if (strcmp(key, "brightnessLo") == 0 && type == 'u') {
        if (parseUint32(value, &u32) && u32 <= 255) {
            Globals::brightnessLo = static_cast<uint8_t>(u32);
            Serial.printf("[Globals] brightnessLo = %u\n", Globals::brightnessLo);
        }
    }
    else if (strcmp(key, "brightnessHi") == 0 && type == 'u') {
        if (parseUint32(value, &u32) && u32 <= 255) {
            Globals::brightnessHi = static_cast<uint8_t>(u32);
            Serial.printf("[Globals] brightnessHi = %u\n", Globals::brightnessHi);
        }
    }
    else if (strcmp(key, "luxShiftLo") == 0 && type == 'i') {
        int32_t i32;
        if (parseInt32(value, &i32) && i32 >= -100 && i32 <= 100) {
            Globals::luxShiftLo = static_cast<int8_t>(i32);
            Serial.printf("[Globals] luxShiftLo = %d\n", Globals::luxShiftLo);
        }
    }
    else if (strcmp(key, "luxShiftHi") == 0 && type == 'i') {
        int32_t i32;
        if (parseInt32(value, &i32) && i32 >= -100 && i32 <= 100) {
            Globals::luxShiftHi = static_cast<int8_t>(i32);
            Serial.printf("[Globals] luxShiftHi = %d\n", Globals::luxShiftHi);
        }
    }
    else if (strcmp(key, "calendarShiftLo") == 0 && type == 'i') {
        int32_t i32;
        if (parseInt32(value, &i32) && i32 >= -100 && i32 <= 100) {
            Globals::calendarShiftLo = static_cast<int8_t>(i32);
            Serial.printf("[Globals] calendarShiftLo = %d\n", Globals::calendarShiftLo);
        }
    }
    else if (strcmp(key, "calendarShiftHi") == 0 && type == 'i') {
        int32_t i32;
        if (parseInt32(value, &i32) && i32 >= -100 && i32 <= 100) {
            Globals::calendarShiftHi = static_cast<int8_t>(i32);
            Serial.printf("[Globals] calendarShiftHi = %d\n", Globals::calendarShiftHi);
        }
    }
    else if (strcmp(key, "maxMilliamps") == 0 && type == 'u') {
        if (parseUint32(value, &u32) && u32 <= 65535) {
            Globals::maxMilliamps = static_cast<uint16_t>(u32);
            Serial.printf("[Globals] maxMilliamps = %u\n", Globals::maxMilliamps);
        }
    }
    // ═══════════════════════════════════════════════════════════
    // SENSORS
    // ═══════════════════════════════════════════════════════════
    else if (strcmp(key, "luxMeasurementDelayMs") == 0 && type == 'u') {
        if (parseUint32(value, &u32)) {
            Globals::luxMeasurementDelayMs = u32;
            Serial.printf("[Globals] luxMeasurementDelayMs = %lu\n", (unsigned long)u32);
        }
    }
    else if (strcmp(key, "luxMeasurementIntervalMs") == 0 && type == 'u') {
        if (parseUint32(value, &u32)) {
            Globals::luxMeasurementIntervalMs = u32;
            Serial.printf("[Globals] luxMeasurementIntervalMs = %lu\n", (unsigned long)u32);
        }
    }
    else if (strcmp(key, "sensorBaseDefaultMs") == 0 && type == 'u') {
        if (parseUint32(value, &u32) && u32 <= 65535) {
            Globals::sensorBaseDefaultMs = static_cast<uint16_t>(u32);
            Serial.printf("[Globals] sensorBaseDefaultMs = %u\n", Globals::sensorBaseDefaultMs);
        }
    }
    else if (strcmp(key, "sensorFastIntervalMs") == 0 && type == 'u') {
        if (parseUint32(value, &u32) && u32 <= 65535) {
            Globals::sensorFastIntervalMs = static_cast<uint16_t>(u32);
            Serial.printf("[Globals] sensorFastIntervalMs = %u\n", Globals::sensorFastIntervalMs);
        }
    }
    else if (strcmp(key, "sensorFastDurationMs") == 0 && type == 'u') {
        if (parseUint32(value, &u32) && u32 <= 65535) {
            Globals::sensorFastDurationMs = static_cast<uint16_t>(u32);
            Serial.printf("[Globals] sensorFastDurationMs = %u\n", Globals::sensorFastDurationMs);
        }
    }
    else if (strcmp(key, "sensorFastDeltaMm") == 0 && type == 'f') {
        if (parseFloat(value, &f32)) {
            Globals::sensorFastDeltaMm = f32;
            Serial.printf("[Globals] sensorFastDeltaMm = %.1f\n", f32);
        }
    }
    else if (strcmp(key, "distanceNewWindowMs") == 0 && type == 'u') {
        if (parseUint32(value, &u32) && u32 <= 65535) {
            Globals::distanceNewWindowMs = static_cast<uint16_t>(u32);
            Serial.printf("[Globals] distanceNewWindowMs = %u\n", Globals::distanceNewWindowMs);
        }
    }
    else if (strcmp(key, "distanceSensorDummyMm") == 0 && type == 'u') {
        if (parseUint32(value, &u32) && u32 <= 65535) {
            Globals::distanceSensorDummyMm = static_cast<uint16_t>(u32);
            Serial.printf("[Globals] distanceSensorDummyMm = %u\n", Globals::distanceSensorDummyMm);
        }
    }
    else if (strcmp(key, "luxSensorDummyLux") == 0 && type == 'f') {
        if (parseFloat(value, &f32)) {
            Globals::luxSensorDummyLux = f32;
            Serial.printf("[Globals] luxSensorDummyLux = %.2f\n", f32);
        }
    }
    else if (strcmp(key, "sensor3DummyTemp") == 0 && type == 'f') {
        if (parseFloat(value, &f32)) {
            Globals::sensor3DummyTemp = f32;
            Serial.printf("[Globals] sensor3DummyTemp = %.1f\n", f32);
        }
    }
    else if (strcmp(key, "distanceSensorInitDelayMs") == 0 && type == 'u') {
        if (parseUint32(value, &u32) && u32 <= 65535) {
            Globals::distanceSensorInitDelayMs = static_cast<uint16_t>(u32);
            Serial.printf("[Globals] distanceSensorInitDelayMs = %u\n", Globals::distanceSensorInitDelayMs);
        }
    }
    else if (strcmp(key, "distanceSensorInitGrowth") == 0 && type == 'f') {
        if (parseFloat(value, &f32) && f32 >= 1.0f) {
            Globals::distanceSensorInitGrowth = f32;
            Serial.printf("[Globals] distanceSensorInitGrowth = %.2f\n", f32);
        }
    }
    else if (strcmp(key, "luxSensorInitDelayMs") == 0 && type == 'u') {
        if (parseUint32(value, &u32) && u32 <= 65535) {
            Globals::luxSensorInitDelayMs = static_cast<uint16_t>(u32);
            Serial.printf("[Globals] luxSensorInitDelayMs = %u\n", Globals::luxSensorInitDelayMs);
        }
    }
    else if (strcmp(key, "luxSensorInitGrowth") == 0 && type == 'f') {
        if (parseFloat(value, &f32) && f32 >= 1.0f) {
            Globals::luxSensorInitGrowth = f32;
            Serial.printf("[Globals] luxSensorInitGrowth = %.2f\n", f32);
        }
    }
    else if (strcmp(key, "distanceMinMm") == 0 && type == 'f') {
        if (parseFloat(value, &f32)) {
            Globals::distanceMinMm = f32;
            Serial.printf("[Globals] distanceMinMm = %.1f\n", f32);
        }
    }
    else if (strcmp(key, "distanceMaxMm") == 0 && type == 'f') {
        if (parseFloat(value, &f32)) {
            Globals::distanceMaxMm = f32;
            Serial.printf("[Globals] distanceMaxMm = %.1f\n", f32);
        }
    }
    // ═══════════════════════════════════════════════════════════
    // HEARTBEAT
    // ═══════════════════════════════════════════════════════════
    else if (strcmp(key, "heartbeatMinMs") == 0 && type == 'u') {
        if (parseUint32(value, &u32) && u32 <= 65535) {
            Globals::heartbeatMinMs = static_cast<uint16_t>(u32);
            Serial.printf("[Globals] heartbeatMinMs = %u\n", Globals::heartbeatMinMs);
        }
    }
    else if (strcmp(key, "heartbeatMaxMs") == 0 && type == 'u') {
        if (parseUint32(value, &u32) && u32 <= 65535) {
            Globals::heartbeatMaxMs = static_cast<uint16_t>(u32);
            Serial.printf("[Globals] heartbeatMaxMs = %u\n", Globals::heartbeatMaxMs);
        }
    }
    else if (strcmp(key, "heartbeatDefaultMs") == 0 && type == 'u') {
        if (parseUint32(value, &u32) && u32 <= 65535) {
            Globals::heartbeatDefaultMs = static_cast<uint16_t>(u32);
            Serial.printf("[Globals] heartbeatDefaultMs = %u\n", Globals::heartbeatDefaultMs);
        }
    }
    // ═══════════════════════════════════════════════════════════
    // ALERT
    // ═══════════════════════════════════════════════════════════
    else if (strcmp(key, "flashBurstIntervalMs") == 0 && type == 'u') {
        if (parseUint32(value, &u32)) {
            Globals::flashBurstIntervalMs = u32;
            Serial.printf("[Globals] flashBurstIntervalMs = %lu\n", (unsigned long)u32);
        }
    }
    else if (strcmp(key, "flashBurstRepeats") == 0 && type == 'u') {
        if (parseUint32(value, &u32) && u32 <= 255) {
            Globals::flashBurstRepeats = static_cast<uint8_t>(u32);
            Serial.printf("[Globals] flashBurstRepeats = %u\n", Globals::flashBurstRepeats);
        }
    }
    else if (strcmp(key, "flashBurstGrowth") == 0 && type == 'f') {
        if (parseFloat(value, &f32) && f32 >= 1.0f) {
            Globals::flashBurstGrowth = f32;
            Serial.printf("[Globals] flashBurstGrowth = %.2f\n", f32);
        }
    }
    else if (strcmp(key, "reminderIntervalMs") == 0 && type == 'u') {
        if (parseUint32(value, &u32)) {
            Globals::reminderIntervalMs = u32;
            Serial.printf("[Globals] reminderIntervalMs = %lu\n", (unsigned long)u32);
        }
    }
    else if (strcmp(key, "reminderIntervalGrowth") == 0 && type == 'f') {
        if (parseFloat(value, &f32) && f32 >= 1.0f) {
            Globals::reminderIntervalGrowth = f32;
            Serial.printf("[Globals] reminderIntervalGrowth = %.2f\n", f32);
        }
    }
    else if (strcmp(key, "flashCriticalMs") == 0 && type == 'u') {
        if (parseUint32(value, &u32) && u32 <= 65535) {
            Globals::flashCriticalMs = static_cast<uint16_t>(u32);
            Serial.printf("[Globals] flashCriticalMs = %u\n", Globals::flashCriticalMs);
        }
    }
    else if (strcmp(key, "flashNormalMs") == 0 && type == 'u') {
        if (parseUint32(value, &u32) && u32 <= 65535) {
            Globals::flashNormalMs = static_cast<uint16_t>(u32);
            Serial.printf("[Globals] flashNormalMs = %u\n", Globals::flashNormalMs);
        }
    }
    // ═══════════════════════════════════════════════════════════
    // BOOT/CLOCK
    // ═══════════════════════════════════════════════════════════
    else if (strcmp(key, "clockBootstrapIntervalMs") == 0 && type == 'u') {
        if (parseUint32(value, &u32)) {
            Globals::clockBootstrapIntervalMs = u32;
            Serial.printf("[Globals] clockBootstrapIntervalMs = %lu\n", (unsigned long)u32);
        }
    }
    else if (strcmp(key, "ntpFallbackTimeoutMs") == 0 && type == 'u') {
        if (parseUint32(value, &u32)) {
            Globals::ntpFallbackTimeoutMs = u32;
            Serial.printf("[Globals] ntpFallbackTimeoutMs = %lu\n", (unsigned long)u32);
        }
    }
    else if (strcmp(key, "bootPhaseMs") == 0 && type == 'u') {
        if (parseUint32(value, &u32)) {
            Globals::bootPhaseMs = u32;
            Serial.printf("[Globals] bootPhaseMs = %lu\n", (unsigned long)u32);
        }
    }
    // ═══════════════════════════════════════════════════════════
    // NETWORK/FETCH
    // ═══════════════════════════════════════════════════════════
    else if (strcmp(key, "weatherRefreshIntervalMs") == 0 && type == 'u') {
        if (parseUint32(value, &u32)) {
            Globals::weatherRefreshIntervalMs = u32;
            Serial.printf("[Globals] weatherRefreshIntervalMs = %lu\n", (unsigned long)u32);
        }
    }
    else if (strcmp(key, "sunRefreshIntervalMs") == 0 && type == 'u') {
        if (parseUint32(value, &u32)) {
            Globals::sunRefreshIntervalMs = u32;
            Serial.printf("[Globals] sunRefreshIntervalMs = %lu\n", (unsigned long)u32);
        }
    }
    else if (strcmp(key, "calendarRefreshIntervalMs") == 0 && type == 'u') {
        if (parseUint32(value, &u32)) {
            Globals::calendarRefreshIntervalMs = u32;
            Serial.printf("[Globals] calendarRefreshIntervalMs = %lu\n", (unsigned long)u32);
        }
    }
    // ═══════════════════════════════════════════════════════════
    // LOCATION
    // ═══════════════════════════════════════════════════════════
    else if (strcmp(key, "locationLat") == 0 && type == 'f') {
        if (parseFloat(value, &f32)) {
            Globals::locationLat = f32;
            Serial.printf("[Globals] locationLat = %.4f\n", f32);
        }
    }
    else if (strcmp(key, "locationLon") == 0 && type == 'f') {
        if (parseFloat(value, &f32)) {
            Globals::locationLon = f32;
            Serial.printf("[Globals] locationLon = %.4f\n", f32);
        }
    }
    // ═══════════════════════════════════════════════════════════
    // TIME FALLBACK
    // ═══════════════════════════════════════════════════════════
    else if (strcmp(key, "fallbackMonth") == 0 && type == 'u') {
        if (parseUint32(value, &u32) && u32 >= 1 && u32 <= 12) {
            Globals::fallbackMonth = static_cast<uint8_t>(u32);
            Serial.printf("[Globals] fallbackMonth = %u\n", Globals::fallbackMonth);
        }
    }
    else if (strcmp(key, "fallbackDay") == 0 && type == 'u') {
        if (parseUint32(value, &u32) && u32 >= 1 && u32 <= 31) {
            Globals::fallbackDay = static_cast<uint8_t>(u32);
            Serial.printf("[Globals] fallbackDay = %u\n", Globals::fallbackDay);
        }
    }
    else if (strcmp(key, "fallbackHour") == 0 && type == 'u') {
        if (parseUint32(value, &u32) && u32 <= 23) {
            Globals::fallbackHour = static_cast<uint8_t>(u32);
            Serial.printf("[Globals] fallbackHour = %u\n", Globals::fallbackHour);
        }
    }
    else if (strcmp(key, "fallbackYear") == 0 && type == 'u') {
        if (parseUint32(value, &u32) && u32 >= 2020 && u32 <= 2100) {
            Globals::fallbackYear = static_cast<uint16_t>(u32);
            Serial.printf("[Globals] fallbackYear = %u\n", Globals::fallbackYear);
        }
    }
    // ═══════════════════════════════════════════════════════════
    // DEBUG
    // ═══════════════════════════════════════════════════════════
    else if (strcmp(key, "timerStatusIntervalMs") == 0 && type == 'u') {
        if (parseUint32(value, &u32)) {
            Globals::timerStatusIntervalMs = u32;
            Serial.printf("[Globals] timerStatusIntervalMs = %lu\n", (unsigned long)u32);
        }
    }
    else if (strcmp(key, "timeDisplayIntervalMs") == 0 && type == 'u') {
        if (parseUint32(value, &u32)) {
            Globals::timeDisplayIntervalMs = u32;
            Serial.printf("[Globals] timeDisplayIntervalMs = %lu\n", (unsigned long)u32);
        }
    }
    // Unknown key: silently ignore (per spec)
}

// ─────────────────────────────────────────────────────────────
// Globals::begin() - Load CSV overrides
// ─────────────────────────────────────────────────────────────

void Globals::begin() {
    // Check SD availability
    if (!AlertState::isSdOk()) {
        Serial.println("[Globals] SD not available, using defaults");
        return;
    }
    
    // Check file existence
    if (!SDManager::fileExists(CSV_PATH)) {
        Serial.println("[Globals] No globals.csv, using defaults");
        return;
    }
    
    // Open file
    File file = SD.open(CSV_PATH, FILE_READ);
    if (!file) {
        Serial.println("[Globals] Failed to open globals.csv");
        return;
    }
    
    Serial.println("[Globals] Loading globals.csv...");
    
    char line[MAX_LINE_LEN];
    int lineNum = 0;
    
    while (file.available()) {
        // Read line
        size_t len = 0;
        while (file.available() && len < MAX_LINE_LEN - 1) {
            char c = file.read();
            if (c == '\n') break;
            line[len++] = c;
        }
        line[len] = '\0';
        lineNum++;
        
        // Skip empty lines
        if (len == 0) continue;
        
        // Trim trailing CR
        if (len > 0 && line[len - 1] == '\r') {
            line[--len] = '\0';
        }
        
        // Skip empty lines after trim
        if (len == 0) continue;
        
        // Skip leading whitespace for comment detection
        const char* trimmed = line;
        while (*trimmed == ' ' || *trimmed == '\t') trimmed++;
        
        // Skip comments and decorative lines
        if (*trimmed == '#' || *trimmed == '\0') continue;
        if (trimmed[0] == '/' && trimmed[1] == '/') continue;
        if (*trimmed == '\xE2' || *trimmed == '=' || *trimmed == '-') continue;
        
        // Parse: key;type;value;comment
        char key[32], typeStr[4], value[64];
        
        const char* p0 = trimmed;
        const char* p1 = findField(trimmed, 1);
        const char* p2 = findField(trimmed, 2);
        
        // Silent skip if not a data line (needs at least 2 semicolons)
        if (!p1 || !p2) continue;
        
        extractField(p0, key, sizeof(key));
        extractField(p1, typeStr, sizeof(typeStr));
        extractField(p2, value, sizeof(value));
        
        if (strlen(key) == 0 || strlen(typeStr) != 1) {
            Serial.printf("[Globals] Line %d: invalid key/type\n", lineNum);
            continue;
        }
        
        applyOverride(key, typeStr[0], value);
    }
    
    file.close();
    Serial.println("[Globals] CSV loading complete");
}

void bootRandomSeed() {
    uint32_t seed = esp_random() ^ micros();
    randomSeed(seed);
}

