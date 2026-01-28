/**
 * @file NotifyState.cpp
 * @brief Hardware status state storage implementation
 * @version 251231E
 * @date 2025-12-31
 *
 * Implements hardware status tracking using a single uint64_t with 4-bit fields.
 * Each component uses 4 bits: 0=OK, 1-14=retries, 15=NotOK.
 * Triggers TTS failure announcements during boot phase and
 * coordinates RGB flash sequences for active failures.
 */

#define LOCAL_LOG_LEVEL LOG_LEVEL_INFO
#include <Arduino.h>
#include "NotifyState.h"
#include "NotifyPolicy.h"
#include "NotifyRGB.h"
#include "Speak/SpeakConduct.h"
#include "Globals.h"
#include "HWConfig.h"

namespace {

// ===== Storage =====
// Single uint64_t with 16 x 4-bit fields (0=OK, 1-14=retries, 15=failed)
uint64_t bootStatus = 0;
bool bootPhase = true;

// ===== Helpers =====
constexpr uint8_t BITS_PER_FIELD = 4;
constexpr uint64_t FIELD_MASK = 0xF;

inline uint8_t extractField(uint64_t val, uint8_t idx) {
    return (val >> (idx * BITS_PER_FIELD)) & FIELD_MASK;
}

inline uint64_t updateField(uint64_t val, uint8_t idx, uint8_t field) {
    field &= FIELD_MASK;
    uint64_t shift = idx * BITS_PER_FIELD;
    val &= ~(FIELD_MASK << shift);
    val |= (uint64_t(field) << shift);
    return val;
}

} // namespace

namespace NotifyState {

// ===== NEW API (v4) =====

uint8_t get(StatusComponent c) {
    return extractField(bootStatus, c);
}

void set(StatusComponent c, uint8_t value) {
    bootStatus = updateField(bootStatus, c, value);
}

SC_Status getStatus(StatusComponent c) {
    // Check if hardware is present first (optional components)
    if (!isPresent(c)) return SC_Status::ABSENT;
    
    uint8_t v = get(c);
    if (v == 0) return SC_Status::OK;
    if (v == 15) return SC_Status::FAILED;
    if (v == 1) return SC_Status::LAST_TRY;
    return SC_Status::RETRY;
}

bool isPresent(StatusComponent c) {
    switch (c) {
        case SC_RTC:     return RTC_PRESENT;
        case SC_DIST:    return DISTANCE_SENSOR_PRESENT;
        case SC_LUX:     return LUX_SENSOR_PRESENT;
        case SC_SENSOR3: return SENSOR3_PRESENT;
        default:         return true;  // SD, WiFi, NTP, etc always "present"
    }
}

bool isStatusOK(StatusComponent c) {
    return get(c) == STATUS_OK;
}

void setStatusOK(StatusComponent c, bool status) {
    set(c, status ? STATUS_OK : STATUS_NOTOK);
}

uint64_t getBootStatus() {
    return bootStatus;
}

// ===== LEGACY API =====

void reset() {
    bootStatus = 0;
    // STATUS_OK=0, so bootStatus=0 means all OK - must explicitly set NOT OK
    setStatusOK(SC_SD, false);       // Not yet mounted
    setStatusOK(SC_WIFI, false);     // Not yet connected
    setStatusOK(SC_RTC, false);      // Not yet probed
    setStatusOK(SC_NTP, false);      // Not yet fetched
    setStatusOK(SC_DIST, false);     // Not yet probed
    setStatusOK(SC_LUX, false);      // Not yet probed
    setStatusOK(SC_SENSOR3, false);  // No sensor3 hardware
    setStatusOK(SC_AUDIO, false);    // Not yet initialized
    setStatusOK(SC_WEATHER, false);  // Not yet fetched
    setStatusOK(SC_CALENDAR, false); // Not yet loaded
    setStatusOK(SC_TTS, false);      // Not yet spoken
    bootPhase = true;
}

void setSdStatus(bool status) {
    if (get(SC_SD) == (status ? STATUS_OK : STATUS_NOTOK)) return;
    setStatusOK(SC_SD, status);
    PF("[*State] SD: %s\n", status ? "OK" : "NOTOK");
    if (!status) SpeakConduct::speak(SpeakIntent::SD_FAIL);
}

void setWifiStatus(bool status) {
    if (get(SC_WIFI) == (status ? STATUS_OK : STATUS_NOTOK)) return;
    setStatusOK(SC_WIFI, status);
    PF("[*State] WiFi: %s\n", status ? "OK" : "NOTOK");
    if (!status) SpeakConduct::speak(SpeakIntent::WIFI_FAIL);
}

void setRtcStatus(bool status) {
    if (get(SC_RTC) == (status ? STATUS_OK : STATUS_NOTOK)) return;
    setStatusOK(SC_RTC, status);
    PF("[*State] RTC: %s\n", status ? "OK" : "NOTOK");
    if (!status) SpeakConduct::speak(SpeakIntent::RTC_FAIL);
}

void setNtpStatus(bool status) {
    if (get(SC_NTP) == (status ? STATUS_OK : STATUS_NOTOK)) return;
    setStatusOK(SC_NTP, status);
    PF("[*State] NTP: %s\n", status ? "OK" : "NOTOK");
    if (!status) SpeakConduct::speak(SpeakIntent::NTP_FAIL);
}

void setDistanceSensorStatus(bool status) {
    if (get(SC_DIST) == (status ? STATUS_OK : STATUS_NOTOK)) return;
    setStatusOK(SC_DIST, status);
    PF("[*State] DistanceSensor: %s\n", status ? "OK" : "NOTOK");
    if (!status) {
        SpeakConduct::speak(SpeakIntent::DISTANCE_SENSOR_FAIL);
        if (!bootPhase) NotifyRGB::startFlashing();
    }
}

void setLuxSensorStatus(bool status) {
    if (get(SC_LUX) == (status ? STATUS_OK : STATUS_NOTOK)) return;
    setStatusOK(SC_LUX, status);
    PF("[*State] LuxSensor: %s\n", status ? "OK" : "NOTOK");
    if (!status) {
        SpeakConduct::speak(SpeakIntent::LUX_SENSOR_FAIL);
        if (!bootPhase) NotifyRGB::startFlashing();
    }
}

void setSensor3Status(bool status) {
    if (get(SC_SENSOR3) == (status ? STATUS_OK : STATUS_NOTOK)) return;
    setStatusOK(SC_SENSOR3, status);
    PF("[*State] Sensor3: %s\n", status ? "OK" : "NOTOK");
    if (!status) {
        SpeakConduct::speak(SpeakIntent::SENSOR3_FAIL);
        if (!bootPhase) NotifyRGB::startFlashing();
    }
}

void setAudioStatus(bool status) {
    if (get(SC_AUDIO) == (status ? STATUS_OK : STATUS_NOTOK)) return;
    setStatusOK(SC_AUDIO, status);
    PF("[*State] Audio: %s\n", status ? "OK" : "NOTOK");
}

void setWeatherStatus(bool status) {
    if (get(SC_WEATHER) == (status ? STATUS_OK : STATUS_NOTOK)) return;
    setStatusOK(SC_WEATHER, status);
    PF("[*State] Weather: %s\n", status ? "OK" : "NOTOK");
    if (!status) SpeakConduct::speak(SpeakIntent::WEATHER_FAIL);
}

void setCalendarStatus(bool status) {
    if (get(SC_CALENDAR) == (status ? STATUS_OK : STATUS_NOTOK)) return;
    setStatusOK(SC_CALENDAR, status);
    PF("[*State] Calendar: %s\n", status ? "OK" : "NOTOK");
    if (!status) SpeakConduct::speak(SpeakIntent::CALENDAR_FAIL);
}

void setTtsStatus(bool status) {
    if (get(SC_TTS) == (status ? STATUS_OK : STATUS_NOTOK)) return;
    setStatusOK(SC_TTS, status);
    PF("[*State] TTS: %s\n", status ? "OK" : "NOTOK");
}

void startRuntime() {
    if (!bootPhase) return;
    bootPhase = false;
    PL("[*State] Runtime started");
    
    // Start flashing if any hardware NotOK during boot
    if (!isStatusOK(SC_SD) || !isStatusOK(SC_WIFI) || !isStatusOK(SC_RTC) || !isStatusOK(SC_NTP) ||
        !isStatusOK(SC_DIST) || !isStatusOK(SC_LUX) || !isStatusOK(SC_SENSOR3)) {
        NotifyRGB::startFlashing();
    }
}

bool isSdOk() { 
    #ifdef TEST_FAIL_SD
    return false;
    #endif
    return isStatusOK(SC_SD); 
}
bool isWifiOk() { 
    #ifdef TEST_FAIL_WIFI
    return false;
    #endif
    return isStatusOK(SC_WIFI); 
}
bool isRtcOk() { 
    #ifdef TEST_FAIL_RTC
    return false;
    #endif
    return isStatusOK(SC_RTC); 
}
bool isNtpOk() { 
    #ifdef TEST_FAIL_NTP
    return false;
    #endif
    return isStatusOK(SC_NTP); 
}
bool isDistanceSensorOk() { 
    #ifdef TEST_FAIL_DISTANCE_SENSOR
    return false;
    #endif
    return isStatusOK(SC_DIST); 
}
bool isLuxSensorOk() { 
    #ifdef TEST_FAIL_LUX_SENSOR
    return false;
    #endif
    return isStatusOK(SC_LUX); 
}
bool isSensor3Ok() { 
    #ifdef TEST_FAIL_SENSOR3
    return false;
    #endif
    return isStatusOK(SC_SENSOR3); 
}

bool isBootPhase() {
    return bootPhase;
}

bool isAudioOk() { return isStatusOK(SC_AUDIO); }
bool isWeatherOk() { return isStatusOK(SC_WEATHER); }
bool isCalendarOk() { return isStatusOK(SC_CALENDAR); }
bool isTtsOk() { return isStatusOK(SC_TTS); }

// Gating functions - check prerequisites for specific operations
bool canPlayHeartbeat() {
    return isStatusOK(SC_AUDIO);
}

bool canPlayTTS() {
    return isStatusOK(SC_WIFI) && isStatusOK(SC_AUDIO);
}

bool canPlayMP3Words() {
    return isStatusOK(SC_SD) && isStatusOK(SC_AUDIO);
}

bool canPlayFragment() {
    return isStatusOK(SC_SD) && isStatusOK(SC_AUDIO) && isStatusOK(SC_CALENDAR);
}

bool canFetch() {
    return isStatusOK(SC_WIFI);
}

uint16_t getHealthBits() {
    uint16_t bits = 0;
    for (int i = 0; i < SC_COUNT; i++) {
        if (isStatusOK(static_cast<StatusComponent>(i))) {
            bits |= (1 << i);
        }
    }
    return bits;
}

uint16_t getAbsentBits() {
    uint16_t bits = 0;
    for (int i = 0; i < SC_COUNT; i++) {
        if (!isPresent(static_cast<StatusComponent>(i))) {
            bits |= (1 << i);
        }
    }
    return bits;
}

} // namespace NotifyState
