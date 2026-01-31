/**
 * @file NotifyRun.cpp
 * @brief Hardware failure notification state management implementation
 * @version 251231E
 * @date 2025-12-31
 *
 * Implements notification routing: maps NotifyRequest values to NotifyState
 * updates, triggers RGB failure flash sequences, and coordinates with
 * ContextFlags for hardware failure bit tracking.
 */

#define LOCAL_LOG_LEVEL LOG_LEVEL_INFO
#include "NotifyRun.h"
#include "NotifyState.h"
#include "NotifyPolicy.h"
#include "NotifyRGB.h"
#include "Audio/AudioPolicy.h"
#include "Speak/SpeakRun.h"
#include "TimerManager.h"
#include "ContextFlags.h"
#include "Globals.h"
#include "SD/SDBoot.h"

namespace {

const char* requestName(NotifyRequest request) {
    switch (request) {
        case NotifyRequest::SD_OK:        return "SD_OK";
        case NotifyRequest::SD_FAIL:      return "SD_FAIL";
        case NotifyRequest::WIFI_OK:      return "WIFI_OK";
        case NotifyRequest::WIFI_FAIL:    return "WIFI_FAIL";
        case NotifyRequest::RTC_OK:       return "RTC_OK";
        case NotifyRequest::RTC_FAIL:     return "RTC_FAIL";
        case NotifyRequest::NTP_OK:       return "NTP_OK";
        case NotifyRequest::NTP_FAIL:     return "NTP_FAIL";
        case NotifyRequest::DISTANCE_SENSOR_OK:   return "DISTANCE_SENSOR_OK";
        case NotifyRequest::DISTANCE_SENSOR_FAIL: return "DISTANCE_SENSOR_FAIL";
        case NotifyRequest::LUX_SENSOR_OK:   return "LUX_SENSOR_OK";
        case NotifyRequest::LUX_SENSOR_FAIL: return "LUX_SENSOR_FAIL";
        case NotifyRequest::SENSOR3_OK:   return "SENSOR3_OK";
        case NotifyRequest::SENSOR3_FAIL: return "SENSOR3_FAIL";
        case NotifyRequest::TTS_OK:       return "TTS_OK";
        case NotifyRequest::TTS_FAIL:     return "TTS_FAIL";
        case NotifyRequest::START_RUNTIME:return "START_RUNTIME";
        default:                         return "UNKNOWN";;
    }
}

void cb_statusReminder() {
    uint64_t failBits = ContextFlags::getHardwareFailBits();
    if (failBits == 0) return;
    
    PF("[*Run] Reminder: failures exist (0x%llX)\n", failBits);
    NotifyRGB::startFlashing();
    
    // Queue only truly FAILED components - not ones still retrying
        if (NotifyState::getStatus(SC_SD) == SC_Status::FAILED)      SpeakRun::speak(SpeakRequest::SD_FAIL);
        if (NotifyState::getStatus(SC_WIFI) == SC_Status::FAILED)    SpeakRun::speak(SpeakRequest::WIFI_FAIL);
        if (NotifyState::getStatus(SC_RTC) == SC_Status::FAILED)     SpeakRun::speak(SpeakRequest::RTC_FAIL);
        if (NotifyState::getStatus(SC_DIST) == SC_Status::FAILED)    SpeakRun::speak(SpeakRequest::DISTANCE_SENSOR_FAIL);
        if (NotifyState::getStatus(SC_LUX) == SC_Status::FAILED)     SpeakRun::speak(SpeakRequest::LUX_SENSOR_FAIL);
        if (NotifyState::getStatus(SC_SENSOR3) == SC_Status::FAILED) SpeakRun::speak(SpeakRequest::SENSOR3_FAIL);
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
        uint8_t v = NotifyState::get(item.c);
        SC_Status s = NotifyState::getStatus(item.c);
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

void NotifyRun::plan() {
    PL("[*Run] plan()");
    NotifyPolicy::configure();
    NotifyState::reset();
    
    // Health status timer
    timers.create(Globals::healthStatusIntervalMs, 0, cb_healthStatus);
}

void NotifyRun::report(NotifyRequest request) {
    PF("[*Run] %s\n", requestName(request));
    
    switch (request) {
        case NotifyRequest::SD_OK:       NotifyState::setSdStatus(true); break;
        case NotifyRequest::SD_FAIL:     NotifyState::setSdStatus(false); break;
        case NotifyRequest::WIFI_OK:
            NotifyState::setWifiStatus(true);
            // WELCOME waits for clock (NTP_OK or RTC_OK)
            break;
        case NotifyRequest::WIFI_FAIL:   NotifyState::setWifiStatus(false); break;
        case NotifyRequest::RTC_OK:
            NotifyState::setRtcStatus(true);
            SDBoot::onTimeAvailable();  // Trigger deferred index rebuild
            // Stage 2: Clock ready → queue welcome (if TTS ready)
            if (NotifyState::canPlayTTS()) {
                SpeakRun::speak(SpeakRequest::WELCOME);
            }
            break;
        case NotifyRequest::RTC_FAIL:    NotifyState::setRtcStatus(false); break;
        case NotifyRequest::NTP_OK:
            NotifyState::setNtpStatus(true);
            SDBoot::onTimeAvailable();  // Trigger deferred index rebuild
            // Stage 2: Clock ready → queue welcome (if TTS ready)
            if (NotifyState::canPlayTTS()) {
                SpeakRun::speak(SpeakRequest::WELCOME);
            }
            break;
        case NotifyRequest::NTP_FAIL:    NotifyState::setNtpStatus(false); break;
        case NotifyRequest::DISTANCE_SENSOR_OK:  NotifyState::setDistanceSensorStatus(true); break;
        case NotifyRequest::DISTANCE_SENSOR_FAIL:NotifyState::setDistanceSensorStatus(false); break;
        case NotifyRequest::LUX_SENSOR_OK:  NotifyState::setLuxSensorStatus(true); break;
        case NotifyRequest::LUX_SENSOR_FAIL:NotifyState::setLuxSensorStatus(false); break;
        case NotifyRequest::SENSOR3_OK:  NotifyState::setSensor3Status(true); break;
        case NotifyRequest::SENSOR3_FAIL:NotifyState::setSensor3Status(false); break;
        case NotifyRequest::TTS_OK:      NotifyState::setTtsStatus(true); break;
        case NotifyRequest::TTS_FAIL:    NotifyState::setTtsStatus(false); break;
        case NotifyRequest::START_RUNTIME: 
            NotifyState::startRuntime();
            // Start reminder timer for failure status flash (exponential backoff)
            timers.create(Globals::reminderIntervalMs, 0, cb_statusReminder, Globals::reminderIntervalGrowth);
            // Welkom queued at WIFI_OK (stage 2), not here
            break;
    }
}

void NotifyRun::speakOnFail(StatusComponent c) {
    if (NotifyState::getStatus(c) == SC_Status::LAST_TRY) {
        NotifyState::set(c, 15);  // → FAILED
            SpeakRun::speakFail(c);
    }
}
