/**
 * @file AlertRun.cpp
 * @brief Hardware failure alert state management implementation
 * @version 251231E
 * @date 2025-12-31
 *
 * Implements alert routing: maps AlertRequest values to AlertState
 * updates, triggers RGB failure flash sequences, and coordinates with
 * StatusFlags for hardware failure bit tracking.
 */

#define LOCAL_LOG_LEVEL LOG_LEVEL_INFO
#include "AlertRun.h"
#include "AlertState.h"
#include "AlertPolicy.h"
#include "AlertRGB.h"
#include "Audio/AudioPolicy.h"
#include "Speak/SpeakRun.h"
#include "TimerManager.h"
#include "StatusFlags.h"
#include "Globals.h"
#include "SD/SDBoot.h"

namespace {

const char* requestName(AlertRequest request) {
    switch (request) {
        case AlertRequest::SD_OK:        return "SD_OK";
        case AlertRequest::SD_FAIL:      return "SD_FAIL";
        case AlertRequest::WIFI_OK:      return "WIFI_OK";
        case AlertRequest::WIFI_FAIL:    return "WIFI_FAIL";
        case AlertRequest::RTC_OK:       return "RTC_OK";
        case AlertRequest::RTC_FAIL:     return "RTC_FAIL";
        case AlertRequest::NTP_OK:       return "NTP_OK";
        case AlertRequest::NTP_FAIL:     return "NTP_FAIL";
        case AlertRequest::DISTANCE_SENSOR_OK:   return "DISTANCE_SENSOR_OK";
        case AlertRequest::DISTANCE_SENSOR_FAIL: return "DISTANCE_SENSOR_FAIL";
        case AlertRequest::LUX_SENSOR_OK:   return "LUX_SENSOR_OK";
        case AlertRequest::LUX_SENSOR_FAIL: return "LUX_SENSOR_FAIL";
        case AlertRequest::SENSOR3_OK:   return "SENSOR3_OK";
        case AlertRequest::SENSOR3_FAIL: return "SENSOR3_FAIL";
        case AlertRequest::TTS_OK:       return "TTS_OK";
        case AlertRequest::TTS_FAIL:     return "TTS_FAIL";
        case AlertRequest::START_RUNTIME:return "START_RUNTIME";
        default:                         return "UNKNOWN";;
    }
}

void cb_statusReminder() {
    uint64_t failBits = StatusFlags::getHardwareFailBits();
    if (failBits == 0) return;
    
    PF("[*Run] Reminder: failures exist (0x%llX)\n", failBits);
    AlertRGB::startFlashing();
    
    // Queue only truly FAILED components - not ones still retrying
    if (AlertState::getStatus(SC_SD) == SC_Status::FAILED)      SpeakRun::speak(SpeakRequest::SD_FAIL);
    if (AlertState::getStatus(SC_WIFI) == SC_Status::FAILED)    SpeakRun::speak(SpeakRequest::WIFI_FAIL);
    if (AlertState::getStatus(SC_RTC) == SC_Status::FAILED)     SpeakRun::speak(SpeakRequest::RTC_FAIL);
    if (AlertState::getStatus(SC_DIST) == SC_Status::FAILED)    SpeakRun::speak(SpeakRequest::DISTANCE_SENSOR_FAIL);
    if (AlertState::getStatus(SC_LUX) == SC_Status::FAILED)     SpeakRun::speak(SpeakRequest::LUX_SENSOR_FAIL);
    if (AlertState::getStatus(SC_SENSOR3) == SC_Status::FAILED) SpeakRun::speak(SpeakRequest::SENSOR3_FAIL);
}

void cb_healthStatus() {
    PF("\n[Health] Version %s\n", FIRMWARE_VERSION);
    PF("[Health] Timers %d/%d\n", timers.getActiveCount(), TimerManager::MAX_TIMERS);
    PL("[Health] Components:");
    
    struct { StatusComponent c; const char* name; const char* icon; } items[] = {
        { SC_SD,       "SD",       "💾" },
        { SC_WIFI,     "WiFi",     "📶" },
        { SC_RTC,      "RTC",      "🕐" },
        { SC_AUDIO,    "Audio",    "🔊" },
        { SC_DIST,     "Distance", "📏" },
        { SC_LUX,      "Lux",      "☀️" },
        { SC_SENSOR3,  "Sensor3",  "🌡️" },
        { SC_NTP,      "NTP",      "⏰" },
        { SC_WEATHER,  "Weather",  "🌤️" },
        { SC_CALENDAR, "Calendar", "📅" },
        { SC_TTS,      "TTS",      "🗣️" },
    };
    
    for (const auto& item : items) {
        uint8_t v = AlertState::get(item.c);
        SC_Status s = AlertState::getStatus(item.c);
        if (s == SC_Status::ABSENT) {
            PF("  %s %-10s —\n", item.icon, item.name);
        } else if (s == SC_Status::OK) {
            PF("  %s %-10s ✅\n", item.icon, item.name);
        } else if (s == SC_Status::FAILED) {
            PF("  %s %-10s ❌\n", item.icon, item.name);
        } else {
            PF("  %s %-10s ⟳ %d\n", item.icon, item.name, v);
        }
    }
}

} // namespace

void AlertRun::plan() {
    PL("[*Run] plan()");
    AlertPolicy::configure();
    AlertState::reset();
    
    // Health status timer
    timers.create(Globals::healthStatusIntervalMs, 0, cb_healthStatus);
}

void AlertRun::report(AlertRequest request) {
    PF("[*Run] %s\n", requestName(request));
    
    switch (request) {
        case AlertRequest::SD_OK:       AlertState::setSdStatus(true); break;
        case AlertRequest::SD_FAIL:     AlertState::setSdStatus(false); break;
        case AlertRequest::WIFI_OK:
            AlertState::setWifiStatus(true);
            // WELCOME waits for clock (NTP_OK or RTC_OK)
            break;
        case AlertRequest::WIFI_FAIL:   AlertState::setWifiStatus(false); break;
        case AlertRequest::RTC_OK:
            AlertState::setRtcStatus(true);
            SDBoot::onTimeAvailable();  // Trigger deferred index rebuild
            // Stage 2: Clock ready → queue welcome (if TTS ready)
            if (AlertState::canPlayTTS()) {
                SpeakRun::speak(SpeakRequest::WELCOME);
            }
            break;
        case AlertRequest::RTC_FAIL:    AlertState::setRtcStatus(false); break;
        case AlertRequest::NTP_OK:
            AlertState::setNtpStatus(true);
            SDBoot::onTimeAvailable();  // Trigger deferred index rebuild
            // Stage 2: Clock ready → queue welcome (if TTS ready)
            if (AlertState::canPlayTTS()) {
                SpeakRun::speak(SpeakRequest::WELCOME);
            }
            break;
        case AlertRequest::NTP_FAIL:    AlertState::setNtpStatus(false); break;
        case AlertRequest::DISTANCE_SENSOR_OK:  AlertState::setDistanceSensorStatus(true); break;
        case AlertRequest::DISTANCE_SENSOR_FAIL:AlertState::setDistanceSensorStatus(false); break;
        case AlertRequest::LUX_SENSOR_OK:  AlertState::setLuxSensorStatus(true); break;
        case AlertRequest::LUX_SENSOR_FAIL:AlertState::setLuxSensorStatus(false); break;
        case AlertRequest::SENSOR3_OK:  AlertState::setSensor3Status(true); break;
        case AlertRequest::SENSOR3_FAIL:AlertState::setSensor3Status(false); break;
        case AlertRequest::TTS_OK:      AlertState::setTtsStatus(true); break;
        case AlertRequest::TTS_FAIL:    AlertState::setTtsStatus(false); break;
        case AlertRequest::START_RUNTIME: 
            AlertState::startRuntime();
            // Start reminder timer for failure status flash (exponential backoff)
            timers.create(Globals::reminderIntervalMs, 0, cb_statusReminder, Globals::reminderIntervalGrowth);
            // Welkom queued at WIFI_OK (stage 2), not here
            break;
    }
}

void AlertRun::speakOnFail(StatusComponent c) {
    if (AlertState::getStatus(c) == SC_Status::LAST_TRY) {
        AlertState::set(c, 15);  // → FAILED
        SpeakRun::speakFail(c);
    }
}