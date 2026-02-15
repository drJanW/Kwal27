/**
 * @file Globals.cpp
 * @brief CSV override loader for Globals
 * @version 260215E
 * @date 2026-02-15
 */
#include "Arduino.h"
#include "Globals.h"
#include "SDController.h"
#include "Alert/AlertState.h"
#include "SdPathUtils.h"
#include <esp_system.h>
#include <math.h>

// Hardware status register (graceful degradation)
uint16_t hwStatus = 0;

// ─────────────────────────────────────────────────────────────
// CSV Parser helpers (C-style, no Arduino String, heap-safe)
// ─────────────────────────────────────────────────────────────

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

static bool setCsvBaseUrl(const char* value) {
    if (!value || !*value) return false;

    const size_t maxLen = sizeof(Globals::csvBaseUrl);
    const size_t len = strlen(value);
    if (len >= maxLen) return false;

    strncpy(Globals::csvBaseUrl, value, maxLen);
    Globals::csvBaseUrl[maxLen - 1] = '\0';

    size_t finalLen = strlen(Globals::csvBaseUrl);
    if (finalLen > 0 && Globals::csvBaseUrl[finalLen - 1] != '/') {
        if (finalLen + 1 >= maxLen) return false;
        Globals::csvBaseUrl[finalLen] = '/';
        Globals::csvBaseUrl[finalLen + 1] = '\0';
    }

    return true;
}

// ─────────────────────────────────────────────────────────────
// Override lookup: match key → apply value
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
            PF_BOOT("[Globals] minAudioIntervalMs = %lu\n", (unsigned long)u32);
        }
    }
    else if (strcmp(key, "maxAudioIntervalMs") == 0 && type == 'u') {
        if (parseUint32(value, &u32)) {
            Globals::maxAudioIntervalMs = u32;
            PF_BOOT("[Globals] maxAudioIntervalMs = %lu\n", (unsigned long)u32);
        }
    }
    else if (strcmp(key, "singleDirMinIntervalMs") == 0 && type == 'u') {
        if (parseUint32(value, &u32)) {
            Globals::singleDirMinIntervalMs = u32;
            PF_BOOT("[Globals] singleDirMinIntervalMs = %lu\n", (unsigned long)u32);
        }
    }
    else if (strcmp(key, "singleDirMaxIntervalMs") == 0 && type == 'u') {
        if (parseUint32(value, &u32)) {
            Globals::singleDirMaxIntervalMs = u32;
            PF_BOOT("[Globals] singleDirMaxIntervalMs = %lu\n", (unsigned long)u32);
        }
    }
    else if (strcmp(key, "baseFadeMs") == 0 && type == 'u') {
        if (parseUint32(value, &u32) && u32 <= 65535) {
            Globals::baseFadeMs = static_cast<uint16_t>(u32);
            PF_BOOT("[Globals] baseFadeMs = %u\n", Globals::baseFadeMs);
        }
    }
    else if (strcmp(key, "webAudioNextFadeMs") == 0 && type == 'u') {
        if (parseUint32(value, &u32) && u32 <= 65535) {
            Globals::webAudioNextFadeMs = static_cast<uint16_t>(u32);
            PF_BOOT("[Globals] webAudioNextFadeMs = %u\n", Globals::webAudioNextFadeMs);
        }
    }
    else if (strcmp(key, "fragmentStartFraction") == 0 && type == 'u') {
        if (parseUint32(value, &u32) && u32 <= 100) {
            Globals::fragmentStartFraction = static_cast<uint8_t>(u32);
            PF_BOOT("[Globals] fragmentStartFraction = %u\n", Globals::fragmentStartFraction);
        }
    }
    else if (strcmp(key, "volumeLo") == 0 && type == 'f') {
        if (parseFloat(value, &f32)) {
            Globals::volumeLo = f32;
            PF_BOOT("[Globals] volumeLo = %.3f\n", f32);
        }
    }
    else if (strcmp(key, "basePlaybackVolume") == 0 && type == 'f') {
        if (parseFloat(value, &f32)) {
            Globals::basePlaybackVolume = f32;
            PF_BOOT("[Globals] basePlaybackVolume = %.3f\n", f32);
        }
    }
    else if (strcmp(key, "minDistanceVolume") == 0 && type == 'f') {
        if (parseFloat(value, &f32)) {
            Globals::minDistanceVolume = f32;
            PF_BOOT("[Globals] minDistanceVolume = %.3f\n", f32);
        }
    }
    else if (strcmp(key, "pingVolumeMax") == 0 && type == 'f') {
        if (parseFloat(value, &f32)) {
            Globals::pingVolumeMax = f32;
            PF_BOOT("[Globals] pingVolumeMax = %.3f\n", f32);
        }
    }
    else if (strcmp(key, "pingVolumeMin") == 0 && type == 'f') {
        if (parseFloat(value, &f32)) {
            Globals::pingVolumeMin = f32;
            PF_BOOT("[Globals] pingVolumeMin = %.3f\n", f32);
        }
    }
    else if (strcmp(key, "busyRetryMs") == 0 && type == 'u') {
        if (parseUint32(value, &u32) && u32 <= 65535) {
            Globals::busyRetryMs = static_cast<uint16_t>(u32);
            PF_BOOT("[Globals] busyRetryMs = %u\n", Globals::busyRetryMs);
        }
    }
    else if (strcmp(key, "defaultAudioSliderPct") == 0 && type == 'u') {
        if (parseUint32(value, &u32) && u32 <= 100) {
            Globals::defaultAudioSliderPct = static_cast<uint8_t>(u32);
            PF_BOOT("[Globals] defaultAudioSliderPct = %u\n", Globals::defaultAudioSliderPct);
        }
    }
    // ═══════════════════════════════════════════════════════════
    // SPEECH
    // ═══════════════════════════════════════════════════════════
    else if (strcmp(key, "minSaytimeIntervalMs") == 0 && type == 'u') {
        if (parseUint32(value, &u32)) {
            Globals::minSaytimeIntervalMs = u32;
            PF_BOOT("[Globals] minSaytimeIntervalMs = %lu\n", (unsigned long)u32);
        }
    }
    else if (strcmp(key, "maxSaytimeIntervalMs") == 0 && type == 'u') {
        if (parseUint32(value, &u32)) {
            Globals::maxSaytimeIntervalMs = u32;
            PF_BOOT("[Globals] maxSaytimeIntervalMs = %lu\n", (unsigned long)u32);
        }
    }
    else if (strcmp(key, "minTemperatureSpeakIntervalMs") == 0 && type == 'u') {
        if (parseUint32(value, &u32)) {
            Globals::minTemperatureSpeakIntervalMs = u32;
            PF_BOOT("[Globals] minTemperatureSpeakIntervalMs = %lu\n", (unsigned long)u32);
        }
    }
    else if (strcmp(key, "maxTemperatureSpeakIntervalMs") == 0 && type == 'u') {
        if (parseUint32(value, &u32)) {
            Globals::maxTemperatureSpeakIntervalMs = u32;
            PF_BOOT("[Globals] maxTemperatureSpeakIntervalMs = %lu\n", (unsigned long)u32);
        }
    }
    // ═══════════════════════════════════════════════════════════
    // LIGHT/PATTERN
    // ═══════════════════════════════════════════════════════════
    else if (strcmp(key, "lightFallbackIntervalMs") == 0 && type == 'u') {
        if (parseUint32(value, &u32) && u32 <= 65535) {
            Globals::lightFallbackIntervalMs = static_cast<uint16_t>(u32);
            PF_BOOT("[Globals] lightFallbackIntervalMs = %u\n", Globals::lightFallbackIntervalMs);
        }
    }
    else if (strcmp(key, "shiftCheckIntervalMs") == 0 && type == 'u') {
        if (parseUint32(value, &u32)) {
            Globals::shiftCheckIntervalMs = u32;
            PF_BOOT("[Globals] shiftCheckIntervalMs = %lu\n", (unsigned long)u32);
        }
    }
    else if (strcmp(key, "defaultFadeWidth") == 0 && type == 'f') {
        if (parseFloat(value, &f32)) {
            Globals::defaultFadeWidth = f32;
            PF_BOOT("[Globals] defaultFadeWidth = %.1f\n", f32);
        }
    }
    else if (strcmp(key, "colorChangeIntervalMs") == 0 && type == 'u') {
        if (parseUint32(value, &u32)) {
            Globals::colorChangeIntervalMs = u32;
            PF_BOOT("[Globals] colorChangeIntervalMs = %lu\n", (unsigned long)u32);
        }
    }
    else if (strcmp(key, "patternChangeIntervalMs") == 0 && type == 'u') {
        if (parseUint32(value, &u32)) {
            Globals::patternChangeIntervalMs = u32;
            PF_BOOT("[Globals] patternChangeIntervalMs = %lu\n", (unsigned long)u32);
        }
    }
    else if (strcmp(key, "maxBrightness") == 0 && type == 'u') {
        if (parseUint32(value, &u32) && u32 <= 255) {
            Globals::maxBrightness = static_cast<uint8_t>(u32);
            PF_BOOT("[Globals] maxBrightness = %u\n", Globals::maxBrightness);
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
            PF_BOOT("[Globals] luxMin = %.1f\n", f32);
        }
    }
    else if (strcmp(key, "luxMax") == 0 && type == 'f') {
        if (parseFloat(value, &f32)) {
            Globals::luxMax = f32;
            PF_BOOT("[Globals] luxMax = %.1f\n", f32);
        }
    }
    else if (strcmp(key, "brightnessLo") == 0 && type == 'u') {
        if (parseUint32(value, &u32) && u32 <= 255) {
            Globals::brightnessLo = static_cast<uint8_t>(u32);
            PF_BOOT("[Globals] brightnessLo = %u\n", Globals::brightnessLo);
        }
    }
    else if (strcmp(key, "brightnessHi") == 0 && type == 'u') {
        if (parseUint32(value, &u32) && u32 <= 255) {
            Globals::brightnessHi = static_cast<uint8_t>(u32);
            PF_BOOT("[Globals] brightnessHi = %u\n", Globals::brightnessHi);
        }
    }
    else if (strcmp(key, "defaultBrightnessSliderPct") == 0 && type == 'u') {
        if (parseUint32(value, &u32) && u32 <= 100) {
            Globals::defaultBrightnessSliderPct = static_cast<uint8_t>(u32);
            PF_BOOT("[Globals] defaultBrightnessSliderPct = %u\n", Globals::defaultBrightnessSliderPct);
        }
    }
    else if (strcmp(key, "luxShiftLo") == 0 && type == 'i') {
        int32_t i32;
        if (parseInt32(value, &i32) && i32 >= -100 && i32 <= 100) {
            Globals::luxShiftLo = static_cast<int8_t>(i32);
            PF_BOOT("[Globals] luxShiftLo = %d\n", Globals::luxShiftLo);
        }
    }
    else if (strcmp(key, "luxShiftHi") == 0 && type == 'i') {
        int32_t i32;
        if (parseInt32(value, &i32) && i32 >= -100 && i32 <= 100) {
            Globals::luxShiftHi = static_cast<int8_t>(i32);
            PF_BOOT("[Globals] luxShiftHi = %d\n", Globals::luxShiftHi);
        }
    }
    else if (strcmp(key, "luxGamma") == 0 && type == 'f') {
        if (parseFloat(value, &f32) && f32 > 0.0f && f32 <= 2.0f) {
            Globals::luxGamma = f32;
            PF_BOOT("[Globals] luxGamma = %.2f\n", static_cast<double>(f32));
        }
    }
    else if (strcmp(key, "calendarShiftLo") == 0 && type == 'i') {
        int32_t i32;
        if (parseInt32(value, &i32) && i32 >= -100 && i32 <= 100) {
            Globals::calendarShiftLo = static_cast<int8_t>(i32);
            PF_BOOT("[Globals] calendarShiftLo = %d\n", Globals::calendarShiftLo);
        }
    }
    else if (strcmp(key, "calendarShiftHi") == 0 && type == 'i') {
        int32_t i32;
        if (parseInt32(value, &i32) && i32 >= -100 && i32 <= 100) {
            Globals::calendarShiftHi = static_cast<int8_t>(i32);
            PF_BOOT("[Globals] calendarShiftHi = %d\n", Globals::calendarShiftHi);
        }
    }
    else if (strcmp(key, "maxMilliamps") == 0 && type == 'u') {
        if (parseUint32(value, &u32) && u32 <= 65535) {
            Globals::maxMilliamps = static_cast<uint16_t>(u32);
            PF_BOOT("[Globals] maxMilliamps = %u\n", Globals::maxMilliamps);
        }
    }
    // ═══════════════════════════════════════════════════════════
    // SENSORS
    // ═══════════════════════════════════════════════════════════
    else if (strcmp(key, "luxMeasurementDelayMs") == 0 && type == 'u') {
        if (parseUint32(value, &u32)) {
            Globals::luxMeasurementDelayMs = u32;
            PF_BOOT("[Globals] luxMeasurementDelayMs = %lu\n", (unsigned long)u32);
        }
    }
    else if (strcmp(key, "luxMeasurementIntervalMs") == 0 && type == 'u') {
        if (parseUint32(value, &u32)) {
            Globals::luxMeasurementIntervalMs = u32;
            PF_BOOT("[Globals] luxMeasurementIntervalMs = %lu\n", (unsigned long)u32);
        }
    }
    else if (strcmp(key, "sensorBaseDefaultMs") == 0 && type == 'u') {
        if (parseUint32(value, &u32) && u32 <= 65535) {
            Globals::sensorBaseDefaultMs = static_cast<uint16_t>(u32);
            PF_BOOT("[Globals] sensorBaseDefaultMs = %u\n", Globals::sensorBaseDefaultMs);
        }
    }
    else if (strcmp(key, "sensorFastIntervalMs") == 0 && type == 'u') {
        if (parseUint32(value, &u32) && u32 <= 65535) {
            Globals::sensorFastIntervalMs = static_cast<uint16_t>(u32);
            PF_BOOT("[Globals] sensorFastIntervalMs = %u\n", Globals::sensorFastIntervalMs);
        }
    }
    else if (strcmp(key, "sensorFastDurationMs") == 0 && type == 'u') {
        if (parseUint32(value, &u32) && u32 <= 65535) {
            Globals::sensorFastDurationMs = static_cast<uint16_t>(u32);
            PF_BOOT("[Globals] sensorFastDurationMs = %u\n", Globals::sensorFastDurationMs);
        }
    }
    else if (strcmp(key, "sensorFastDeltaMm") == 0 && type == 'f') {
        if (parseFloat(value, &f32)) {
            Globals::sensorFastDeltaMm = f32;
            PF_BOOT("[Globals] sensorFastDeltaMm = %.1f\n", f32);
        }
    }
    else if (strcmp(key, "distanceNewWindowMs") == 0 && type == 'u') {
        if (parseUint32(value, &u32) && u32 <= 65535) {
            Globals::distanceNewWindowMs = static_cast<uint16_t>(u32);
            PF_BOOT("[Globals] distanceNewWindowMs = %u\n", Globals::distanceNewWindowMs);
        }
    }
    else if (strcmp(key, "distanceSensorDummyMm") == 0 && type == 'u') {
        if (parseUint32(value, &u32) && u32 <= 65535) {
            Globals::distanceSensorDummyMm = static_cast<uint16_t>(u32);
            PF_BOOT("[Globals] distanceSensorDummyMm = %u\n", Globals::distanceSensorDummyMm);
        }
    }
    else if (strcmp(key, "luxSensorDummyLux") == 0 && type == 'f') {
        if (parseFloat(value, &f32)) {
            Globals::luxSensorDummyLux = f32;
            PF_BOOT("[Globals] luxSensorDummyLux = %.2f\n", f32);
        }
    }
    else if (strcmp(key, "sensor3DummyTemp") == 0 && type == 'f') {
        if (parseFloat(value, &f32)) {
            Globals::sensor3DummyTemp = f32;
            PF_BOOT("[Globals] sensor3DummyTemp = %.1f\n", f32);
        }
    }
    else if (strcmp(key, "distanceSensorInitDelayMs") == 0 && type == 'u') {
        if (parseUint32(value, &u32) && u32 <= 65535) {
            Globals::distanceSensorInitDelayMs = static_cast<uint16_t>(u32);
            PF_BOOT("[Globals] distanceSensorInitDelayMs = %u\n", Globals::distanceSensorInitDelayMs);
        }
    }
    else if (strcmp(key, "distanceSensorInitGrowth") == 0 && type == 'f') {
        if (parseFloat(value, &f32) && f32 >= 1.0f) {
            Globals::distanceSensorInitGrowth = f32;
            PF_BOOT("[Globals] distanceSensorInitGrowth = %.2f\n", f32);
        }
    }
    else if (strcmp(key, "luxSensorInitDelayMs") == 0 && type == 'u') {
        if (parseUint32(value, &u32) && u32 <= 65535) {
            Globals::luxSensorInitDelayMs = static_cast<uint16_t>(u32);
            PF_BOOT("[Globals] luxSensorInitDelayMs = %u\n", Globals::luxSensorInitDelayMs);
        }
    }
    else if (strcmp(key, "luxSensorInitGrowth") == 0 && type == 'f') {
        if (parseFloat(value, &f32) && f32 >= 1.0f) {
            Globals::luxSensorInitGrowth = f32;
            PF_BOOT("[Globals] luxSensorInitGrowth = %.2f\n", f32);
        }
    }
    else if (strcmp(key, "distanceMinMm") == 0 && type == 'f') {
        if (parseFloat(value, &f32)) {
            Globals::distanceMinMm = f32;
            PF_BOOT("[Globals] distanceMinMm = %.1f\n", f32);
        }
    }
    else if (strcmp(key, "distanceMaxMm") == 0 && type == 'f') {
        if (parseFloat(value, &f32)) {
            Globals::distanceMaxMm = f32;
            PF_BOOT("[Globals] distanceMaxMm = %.1f\n", f32);
        }
    }
    // ═══════════════════════════════════════════════════════════
    // HEARTBEAT
    // ═══════════════════════════════════════════════════════════
    else if (strcmp(key, "heartbeatMinMs") == 0 && type == 'u') {
        if (parseUint32(value, &u32) && u32 <= 65535) {
            Globals::heartbeatMinMs = static_cast<uint16_t>(u32);
            PF_BOOT("[Globals] heartbeatMinMs = %u\n", Globals::heartbeatMinMs);
        }
    }
    else if (strcmp(key, "heartbeatMaxMs") == 0 && type == 'u') {
        if (parseUint32(value, &u32) && u32 <= 65535) {
            Globals::heartbeatMaxMs = static_cast<uint16_t>(u32);
            PF_BOOT("[Globals] heartbeatMaxMs = %u\n", Globals::heartbeatMaxMs);
        }
    }
    else if (strcmp(key, "heartbeatDefaultMs") == 0 && type == 'u') {
        if (parseUint32(value, &u32) && u32 <= 65535) {
            Globals::heartbeatDefaultMs = static_cast<uint16_t>(u32);
            PF_BOOT("[Globals] heartbeatDefaultMs = %u\n", Globals::heartbeatDefaultMs);
        }
    }
    // ═══════════════════════════════════════════════════════════
    // ALERT
    // ═══════════════════════════════════════════════════════════
    else if (strcmp(key, "flashBurstIntervalMs") == 0 && type == 'u') {
        if (parseUint32(value, &u32)) {
            Globals::flashBurstIntervalMs = u32;
            PF_BOOT("[Globals] flashBurstIntervalMs = %lu\n", (unsigned long)u32);
        }
    }
    else if (strcmp(key, "flashBurstRepeats") == 0 && type == 'u') {
        if (parseUint32(value, &u32) && u32 <= 255) {
            Globals::flashBurstRepeats = static_cast<uint8_t>(u32);
            PF_BOOT("[Globals] flashBurstRepeats = %u\n", Globals::flashBurstRepeats);
        }
    }
    else if (strcmp(key, "flashBurstGrowth") == 0 && type == 'f') {
        if (parseFloat(value, &f32) && f32 >= 1.0f) {
            Globals::flashBurstGrowth = f32;
            PF_BOOT("[Globals] flashBurstGrowth = %.2f\n", f32);
        }
    }
    else if (strcmp(key, "reminderIntervalMs") == 0 && type == 'u') {
        if (parseUint32(value, &u32)) {
            Globals::reminderIntervalMs = u32;
            PF_BOOT("[Globals] reminderIntervalMs = %lu\n", (unsigned long)u32);
        }
    }
    else if (strcmp(key, "reminderIntervalGrowth") == 0 && type == 'f') {
        if (parseFloat(value, &f32) && f32 >= 1.0f) {
            Globals::reminderIntervalGrowth = f32;
            PF_BOOT("[Globals] reminderIntervalGrowth = %.2f\n", f32);
        }
    }
    else if (strcmp(key, "flashCriticalMs") == 0 && type == 'u') {
        if (parseUint32(value, &u32) && u32 <= 65535) {
            Globals::flashCriticalMs = static_cast<uint16_t>(u32);
            PF_BOOT("[Globals] flashCriticalMs = %u\n", Globals::flashCriticalMs);
        }
    }
    else if (strcmp(key, "flashNormalMs") == 0 && type == 'u') {
        if (parseUint32(value, &u32) && u32 <= 65535) {
            Globals::flashNormalMs = static_cast<uint16_t>(u32);
            PF_BOOT("[Globals] flashNormalMs = %u\n", Globals::flashNormalMs);
        }
    }
    // ═══════════════════════════════════════════════════════════
    // BOOT/CLOCK
    // ═══════════════════════════════════════════════════════════
    else if (strcmp(key, "clockBootstrapIntervalMs") == 0 && type == 'u') {
        if (parseUint32(value, &u32)) {
            Globals::clockBootstrapIntervalMs = u32;
            PF_BOOT("[Globals] clockBootstrapIntervalMs = %lu\n", (unsigned long)u32);
        }
    }
    else if (strcmp(key, "ntpFallbackTimeoutMs") == 0 && type == 'u') {
        if (parseUint32(value, &u32)) {
            Globals::ntpFallbackTimeoutMs = u32;
            PF_BOOT("[Globals] ntpFallbackTimeoutMs = %lu\n", (unsigned long)u32);
        }
    }
    else if (strcmp(key, "bootPhaseMs") == 0 && type == 'u') {
        if (parseUint32(value, &u32)) {
            Globals::bootPhaseMs = u32;
            PF_BOOT("[Globals] bootPhaseMs = %lu\n", (unsigned long)u32);
        }
    }
    else if (strcmp(key, "rtcTemperatureIntervalMs") == 0 && type == 'u') {
        if (parseUint32(value, &u32)) {
            Globals::rtcTemperatureIntervalMs = u32;
            PF_BOOT("[Globals] rtcTemperatureIntervalMs = %lu\n", (unsigned long)u32);
        }
    }
    // ═══════════════════════════════════════════════════════════
    // NETWORK/FETCH
    // ═══════════════════════════════════════════════════════════
    else if (strcmp(key, "weatherRefreshIntervalMs") == 0 && type == 'u') {
        if (parseUint32(value, &u32)) {
            Globals::weatherRefreshIntervalMs = u32;
            PF_BOOT("[Globals] weatherRefreshIntervalMs = %lu\n", (unsigned long)u32);
        }
    }
    else if (strcmp(key, "sunRefreshIntervalMs") == 0 && type == 'u') {
        if (parseUint32(value, &u32)) {
            Globals::sunRefreshIntervalMs = u32;
            PF_BOOT("[Globals] sunRefreshIntervalMs = %lu\n", (unsigned long)u32);
        }
    }
    else if (strcmp(key, "calendarRefreshIntervalMs") == 0 && type == 'u') {
        if (parseUint32(value, &u32)) {
            Globals::calendarRefreshIntervalMs = u32;
            PF_BOOT("[Globals] calendarRefreshIntervalMs = %lu\n", (unsigned long)u32);
        }
    }
    // ═══════════════════════════════════════════════════════════
    // CSV HTTP
    // ═══════════════════════════════════════════════════════════
    else if (strcmp(key, "csvBaseUrl") == 0 && type == 's') {
        if (setCsvBaseUrl(value)) {
            PF_BOOT("[Globals] csvBaseUrl = %s\n", Globals::csvBaseUrl);
        }
    }
    else if (strcmp(key, "csvHttpTimeoutMs") == 0 && type == 'u') {
        if (parseUint32(value, &u32)) {
            Globals::csvHttpTimeoutMs = u32;
            PF_BOOT("[Globals] csvHttpTimeoutMs = %lu\n", (unsigned long)u32);
        }
    }
    else if (strcmp(key, "csvFetchWaitMs") == 0 && type == 'u') {
        if (parseUint32(value, &u32)) {
            Globals::csvFetchWaitMs = u32;
            PF_BOOT("[Globals] csvFetchWaitMs = %lu\n", (unsigned long)u32);
        }
    }
    // ═══════════════════════════════════════════════════════════
    // LOCATION
    // ═══════════════════════════════════════════════════════════
    else if (strcmp(key, "locationLat") == 0 && type == 'f') {
        if (parseFloat(value, &f32)) {
            Globals::locationLat = f32;
            PF_BOOT("[Globals] locationLat = %.4f\n", f32);
        }
    }
    else if (strcmp(key, "locationLon") == 0 && type == 'f') {
        if (parseFloat(value, &f32)) {
            Globals::locationLon = f32;
            PF_BOOT("[Globals] locationLon = %.4f\n", f32);
        }
    }
    // ═══════════════════════════════════════════════════════════
    // TIME FALLBACK
    // ═══════════════════════════════════════════════════════════
    else if (strcmp(key, "fallbackMonth") == 0 && type == 'u') {
        if (parseUint32(value, &u32) && u32 >= 1 && u32 <= 12) {
            Globals::fallbackMonth = static_cast<uint8_t>(u32);
            PF_BOOT("[Globals] fallbackMonth = %u\n", Globals::fallbackMonth);
        }
    }
    else if (strcmp(key, "fallbackDay") == 0 && type == 'u') {
        if (parseUint32(value, &u32) && u32 >= 1 && u32 <= 31) {
            Globals::fallbackDay = static_cast<uint8_t>(u32);
            PF_BOOT("[Globals] fallbackDay = %u\n", Globals::fallbackDay);
        }
    }
    else if (strcmp(key, "fallbackHour") == 0 && type == 'u') {
        if (parseUint32(value, &u32) && u32 <= 23) {
            Globals::fallbackHour = static_cast<uint8_t>(u32);
            PF_BOOT("[Globals] fallbackHour = %u\n", Globals::fallbackHour);
        }
    }
    else if (strcmp(key, "fallbackYear") == 0 && type == 'u') {
        if (parseUint32(value, &u32) && u32 >= 2020 && u32 <= 2100) {
            Globals::fallbackYear = static_cast<uint16_t>(u32);
            PF_BOOT("[Globals] fallbackYear = %u\n", Globals::fallbackYear);
        }
    }
    // ═══════════════════════════════════════════════════════════
    // DEBUG
    // ═══════════════════════════════════════════════════════════
    else if (strcmp(key, "timerStatusIntervalMs") == 0 && type == 'u') {
        if (parseUint32(value, &u32)) {
            Globals::timerStatusIntervalMs = u32;
            PF_BOOT("[Globals] timerStatusIntervalMs = %lu\n", (unsigned long)u32);
        }
    }
    // Unknown key: silently ignore (per spec)
}

// ─────────────────────────────────────────────────────────────
// Globals::begin() - Load CSV overrides
// ─────────────────────────────────────────────────────────────

// Sine² fade curve: curve[i] = sin²(π/2 × i/(N-1)), i = 0…14
// Called once from SystemBoot stage 0
float Globals::fadeCurve[Globals::fadeStepCount] = {};

void Globals::fillFadeCurve() {
    for (uint8_t i = 0; i < fadeStepCount; ++i) {
        float x = static_cast<float>(i) / static_cast<float>(fadeStepCount - 1);
        float s = sinf(1.5707963f * x);
        fadeCurve[i] = s * s;
    }
}

// ─────────────────────────────────────────────────────────────
// Load config.txt: key=value pairs for device identity & hardware presence
// ─────────────────────────────────────────────────────────────
static void loadConfigTxt() {
    const String path = SdPathUtils::chooseCsvPath("config.txt");
    if (path.isEmpty() || !SDController::fileExists(path.c_str())) {
        PL("[Globals] No config.txt, using firmware defaults");
        return;
    }
    File file = SD.open(path.c_str(), FILE_READ);
    if (!file) {
        PL("[Globals] Failed to open config.txt");
        return;
    }

    char line[128];
    uint8_t keysLoaded = 0;

    while (file.available()) {
        size_t len = 0;
        while (file.available() && len < sizeof(line) - 1) {
            char c = file.read();
            if (c == '\n') break;
            line[len++] = c;
        }
        line[len] = '\0';
        // Trim CR
        if (len > 0 && line[len - 1] == '\r') line[--len] = '\0';
        // Skip empty lines and comments
        if (len == 0 || line[0] == '#') continue;

        // Split on first '=' (value may contain '=')
        char* eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        const char* key = line;
        const char* val = eq + 1;

        if (strcmp(key, "name") == 0) {
            strncpy(Globals::deviceName, val, sizeof(Globals::deviceName) - 1);
            Globals::deviceName[sizeof(Globals::deviceName) - 1] = '\0';
            PF_BOOT("[Globals] deviceName = %s\n", Globals::deviceName);
        } else if (strcmp(key, "ssid") == 0) {
            strncpy(Globals::wifiSsid, val, sizeof(Globals::wifiSsid) - 1);
            Globals::wifiSsid[sizeof(Globals::wifiSsid) - 1] = '\0';
            PF_BOOT("[Globals] wifiSsid = %s (from config.txt)\n", Globals::wifiSsid);
        } else if (strcmp(key, "password") == 0) {
            strncpy(Globals::wifiPassword, val, sizeof(Globals::wifiPassword) - 1);
            Globals::wifiPassword[sizeof(Globals::wifiPassword) - 1] = '\0';
            PF_BOOT("[Globals] wifiPassword = *** (from config.txt)\n");
        } else if (strcmp(key, "ip") == 0) {
            strncpy(Globals::staticIp, val, sizeof(Globals::staticIp) - 1);
            Globals::staticIp[sizeof(Globals::staticIp) - 1] = '\0';
            PF_BOOT("[Globals] staticIp = %s (from config.txt)\n", Globals::staticIp);
        } else if (strcmp(key, "gateway") == 0) {
            strncpy(Globals::staticGateway, val, sizeof(Globals::staticGateway) - 1);
            Globals::staticGateway[sizeof(Globals::staticGateway) - 1] = '\0';
            PF_BOOT("[Globals] staticGateway = %s (from config.txt)\n", Globals::staticGateway);
        } else if (strcmp(key, "rtc") == 0) {
            Globals::rtcPresent = (val[0] == '1');
            PF_BOOT("[Globals] rtcPresent = %d (from config.txt)\n", Globals::rtcPresent);
        } else if (strcmp(key, "lux") == 0) {
            Globals::luxSensorPresent = (val[0] == '1');
            PF_BOOT("[Globals] luxSensorPresent = %d (from config.txt)\n", Globals::luxSensorPresent);
        } else if (strcmp(key, "distance") == 0) {
            Globals::distanceSensorPresent = (val[0] == '1');
            PF_BOOT("[Globals] distanceSensorPresent = %d (from config.txt)\n", Globals::distanceSensorPresent);
        } else if (strcmp(key, "sensor3") == 0) {
            Globals::sensor3Present = (val[0] == '1');
            PF_BOOT("[Globals] sensor3Present = %d (from config.txt)\n", Globals::sensor3Present);
        }
        keysLoaded++;
    }
    file.close();

    if (keysLoaded < 2) {
        PL("[Globals] config.txt has very few keys - check file");
    }
}

void Globals::begin() {
    // Check SD availability
    if (!AlertState::isSdOk()) {
        Serial.println("[Globals] SD not available, using defaults");
        return;
    }

    // Load device identity and WiFi from config.txt (before globals.csv)
    loadConfigTxt();
    PF("[config] device=%s\n", Globals::deviceName);
    PF("[config] firmware=%s\n", Globals::firmwareVersion);
    
    // Check file existence
    const String csvPath = SdPathUtils::chooseCsvPath("globals.csv");
    if (csvPath.isEmpty() || !SDController::fileExists(csvPath.c_str())) {
        Serial.println("[Globals] No globals.csv, using defaults");
        return;
    }
    
    // Open file
    File file = SD.open(csvPath.c_str(), FILE_READ);
    if (!file) {
        PF("[Globals] Failed to open %s\n", csvPath.c_str());
        return;
    }
    
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
            PF("[Globals] Line %d: invalid key/type\n", lineNum);
            continue;
        }
        
        applyOverride(key, typeStr[0], value);
    }
    
    file.close();
}

void bootRandomSeed() {
    uint32_t seed = esp_random() ^ micros();
    randomSeed(seed);
}

