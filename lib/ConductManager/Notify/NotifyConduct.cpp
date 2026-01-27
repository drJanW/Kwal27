/**
 * @file NotifyConduct.cpp
 * @brief Hardware failure notification state management implementation
 * @version 251231E
 * @date 2025-12-31
 *
 * Implements notification routing: maps NotifyIntent values to NotifyState
 * updates, triggers RGB failure flash sequences, and coordinates with
 * ContextFlags for hardware failure bit tracking.
 */

#define LOCAL_LOG_LEVEL LOG_LEVEL_INFO
#include "NotifyConduct.h"
#include "NotifyState.h"
#include "NotifyPolicy.h"
#include "NotifyRGB.h"
#include "Audio/AudioPolicy.h"
#include "Speak/SpeakConduct.h"
#include "TimerManager.h"
#include "ContextFlags.h"
#include "Globals.h"
#include "SD/SDBoot.h"

namespace {

const char* intentName(NotifyIntent intent) {
    switch (intent) {
        case NotifyIntent::SD_OK:        return "SD_OK";
        case NotifyIntent::SD_FAIL:      return "SD_FAIL";
        case NotifyIntent::WIFI_OK:      return "WIFI_OK";
        case NotifyIntent::WIFI_FAIL:    return "WIFI_FAIL";
        case NotifyIntent::RTC_OK:       return "RTC_OK";
        case NotifyIntent::RTC_FAIL:     return "RTC_FAIL";
        case NotifyIntent::NTP_OK:       return "NTP_OK";
        case NotifyIntent::NTP_FAIL:     return "NTP_FAIL";
        case NotifyIntent::DISTANCE_SENSOR_OK:   return "DISTANCE_SENSOR_OK";
        case NotifyIntent::DISTANCE_SENSOR_FAIL: return "DISTANCE_SENSOR_FAIL";
        case NotifyIntent::LUX_SENSOR_OK:   return "LUX_SENSOR_OK";
        case NotifyIntent::LUX_SENSOR_FAIL: return "LUX_SENSOR_FAIL";
        case NotifyIntent::SENSOR3_OK:   return "SENSOR3_OK";
        case NotifyIntent::SENSOR3_FAIL: return "SENSOR3_FAIL";
        case NotifyIntent::TTS_OK:       return "TTS_OK";
        case NotifyIntent::TTS_FAIL:     return "TTS_FAIL";
        case NotifyIntent::START_RUNTIME:return "START_RUNTIME";
        default:                         return "UNKNOWN";;
    }
}

void cb_statusReminder() {
    uint64_t failBits = ContextFlags::getHardwareFailBits();
    if (failBits == 0) return;
    
    PF("[*Conduct] Reminder: failures exist (0x%llX)\n", failBits);
    NotifyRGB::startFlashing();
    
    // Queue only truly FAILED components - not ones still retrying
    if (NotifyState::getStatus(SC_SD) == SC_Status::FAILED)      SpeakConduct::speak(SpeakIntent::SD_FAIL);
    if (NotifyState::getStatus(SC_WIFI) == SC_Status::FAILED)    SpeakConduct::speak(SpeakIntent::WIFI_FAIL);
    if (NotifyState::getStatus(SC_RTC) == SC_Status::FAILED)     SpeakConduct::speak(SpeakIntent::RTC_FAIL);
    if (NotifyState::getStatus(SC_DIST) == SC_Status::FAILED)    SpeakConduct::speak(SpeakIntent::DISTANCE_SENSOR_FAIL);
    if (NotifyState::getStatus(SC_LUX) == SC_Status::FAILED)     SpeakConduct::speak(SpeakIntent::LUX_SENSOR_FAIL);
    if (NotifyState::getStatus(SC_SENSOR3) == SC_Status::FAILED) SpeakConduct::speak(SpeakIntent::SENSOR3_FAIL);
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

void NotifyConduct::plan() {
    PL("[*Conduct] plan()");
    NotifyPolicy::configure();
    NotifyState::reset();
    
    // Health status timer
    timers.create(Globals::healthStatusIntervalMs, 0, cb_healthStatus);
}

void NotifyConduct::report(NotifyIntent intent) {
    PF("[*Conduct] %s\n", intentName(intent));
    
    switch (intent) {
        case NotifyIntent::SD_OK:       NotifyState::setSdStatus(true); break;
        case NotifyIntent::SD_FAIL:     NotifyState::setSdStatus(false); break;
        case NotifyIntent::WIFI_OK:
            NotifyState::setWifiStatus(true);
            // WELCOME waits for clock (NTP_OK or RTC_OK)
            break;
        case NotifyIntent::WIFI_FAIL:   NotifyState::setWifiStatus(false); break;
        case NotifyIntent::RTC_OK:
            NotifyState::setRtcStatus(true);
            SDBoot::onTimeAvailable();  // Trigger deferred index rebuild
            // Stage 2: Clock ready → queue welcome (if TTS ready)
            if (NotifyState::canPlayTTS()) {
                SpeakConduct::speak(SpeakIntent::WELCOME);
            }
            break;
        case NotifyIntent::RTC_FAIL:    NotifyState::setRtcStatus(false); break;
        case NotifyIntent::NTP_OK:
            NotifyState::setNtpStatus(true);
            SDBoot::onTimeAvailable();  // Trigger deferred index rebuild
            // Stage 2: Clock ready → queue welcome (if TTS ready)
            if (NotifyState::canPlayTTS()) {
                SpeakConduct::speak(SpeakIntent::WELCOME);
            }
            break;
        case NotifyIntent::NTP_FAIL:    NotifyState::setNtpStatus(false); break;
        case NotifyIntent::DISTANCE_SENSOR_OK:  NotifyState::setDistanceSensorStatus(true); break;
        case NotifyIntent::DISTANCE_SENSOR_FAIL:NotifyState::setDistanceSensorStatus(false); break;
        case NotifyIntent::LUX_SENSOR_OK:  NotifyState::setLuxSensorStatus(true); break;
        case NotifyIntent::LUX_SENSOR_FAIL:NotifyState::setLuxSensorStatus(false); break;
        case NotifyIntent::SENSOR3_OK:  NotifyState::setSensor3Status(true); break;
        case NotifyIntent::SENSOR3_FAIL:NotifyState::setSensor3Status(false); break;
        case NotifyIntent::TTS_OK:      NotifyState::setTtsStatus(true); break;
        case NotifyIntent::TTS_FAIL:    NotifyState::setTtsStatus(false); break;
        case NotifyIntent::START_RUNTIME: 
            NotifyState::startRuntime();
            // Start reminder timer for failure status flash (exponential backoff)
            timers.create(Globals::reminderIntervalMs, 0, cb_statusReminder, Globals::reminderIntervalGrowth);
            // Welkom queued at WIFI_OK (stage 2), not here
            break;
    }
}

void NotifyConduct::speakOnFail(StatusComponent c) {
    if (NotifyState::getStatus(c) == SC_Status::LAST_TRY) {
        NotifyState::set(c, 15);  // → FAILED
        SpeakConduct::speakFail(c);
    }
}
