/**
 * @file AlertRun.cpp
 * @brief Hardware failure alert state management implementation
 * @version 260213A
 * @date 2026-02-13
 */
#define LOCAL_LOG_LEVEL LOG_LEVEL_INFO
#include <Arduino.h>
#include "AlertRun.h"
#include "AlertState.h"
#include "AlertPolicy.h"
#include "AlertRGB.h"
#include "Audio/AudioPolicy.h"
#include "Speak/SpeakRun.h"
#include "TodayState.h"
#include "TimerManager.h"
#include "StatusFlags.h"
#include "Globals.h"
#include "ContextController.h"
#include "SD/SDBoot.h"
#include <ESP.h>

namespace {

bool welcomePending = false;

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

bool isFailure(AlertRequest request) {
    switch (request) {
        case AlertRequest::SD_FAIL:
        case AlertRequest::WIFI_FAIL:
        case AlertRequest::RTC_FAIL:
        case AlertRequest::NTP_FAIL:
        case AlertRequest::DISTANCE_SENSOR_FAIL:
        case AlertRequest::LUX_SENSOR_FAIL:
        case AlertRequest::SENSOR3_FAIL:
        case AlertRequest::TTS_FAIL:
            return true;
        default:
            return false;
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
    const auto& ts = ContextController::time();

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
        { SC_NAS,      "NAS",      "🗄️" },
    };
    
    for (const auto& item : items) {
        uint8_t v = AlertState::get(item.c);
        SC_Status s = AlertState::getStatus(item.c);
        if (s == SC_Status::ABSENT) {
            PF("  %s %-10s —\n", item.icon, item.name);
        } else if (s == SC_Status::OK) {
            if (item.c == SC_RTC && ts.hasRtcTemperature) {
                PF("  %s %-10s ✅ %.1f°C\n", item.icon, item.name,
                   static_cast<double>(ts.rtcTemperatureC));
            } else if (item.c == SC_AUDIO) {
                const String& boxId = AudioPolicy::themeBoxId();
                if (!boxId.isEmpty()) {
                    long numId = boxId.toInt();
                    const ThemeBox* tb = (numId > 0) ? FindThemeBox(static_cast<uint8_t>(numId)) : nullptr;
                    if (tb) {
                        PF("  %s %-10s ✅ %s\n", item.icon, item.name, tb->name.c_str());
                    } else {
                        PF("  %s %-10s ✅ [%s]\n", item.icon, item.name, boxId.c_str());
                    }
                } else {
                    PF("  %s %-10s ✅\n", item.icon, item.name);
                }
            } else if (item.c == SC_NTP) {
                PF("  %s %-10s ✅ %02u:%02u\n", item.icon, item.name,
                   ts.hour, ts.minute);
            } else {
                PF("  %s %-10s ✅\n", item.icon, item.name);
            }
        } else if (s == SC_Status::FAILED) {
            PF("  %s %-10s ❌\n", item.icon, item.name);
        } else {
            PF("  %s %-10s ⟳ %d\n", item.icon, item.name, v);
        }
    }

    // Heap: current > minimum (largest block)
    PF("  🧠 Heap       %u>%uKB (%u)\n",
       static_cast<unsigned>(ESP.getFreeHeap() / 1024),
       static_cast<unsigned>(ESP.getMinFreeHeap() / 1024),
       static_cast<unsigned>(ESP.getMaxAllocHeap() / 1024));

    // Timers: max active since boot
    timers.getActiveCount();  // update max
    PF("  ⏱️ Timers     max %d of %d used\n", timers.getMaxActiveTimers(), MAX_TIMERS);
}

} // namespace

void AlertRun::plan() {
    AlertPolicy::configure();
    AlertState::reset();
    
    // Health status timer
    timers.create(Globals::healthStatusIntervalMs, 0, cb_healthStatus);
}

void AlertRun::requestWelcome() {
    welcomePending = true;
}

void AlertRun::playWelcomeIfPending() {
    if (!welcomePending) {
        return;
    }
    welcomePending = false;
    SpeakRun::speak(SpeakRequest::WELCOME);
}

void AlertRun::report(AlertRequest request) {
    if (isFailure(request)) {
        PF("[Alert] %s\n", requestName(request));
    } else {
        PF_BOOT("[*Run] %s\n", requestName(request));
    }
    
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
            SDBoot::onTimeAvailable();
            requestWelcome();
            break;
        case AlertRequest::RTC_FAIL:    AlertState::setRtcStatus(false); break;
        case AlertRequest::NTP_OK:
            AlertState::setNtpStatus(true);
            SDBoot::onTimeAvailable();
            requestWelcome();
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
            // Welcome queued at clock ready, played after calendar is ready
            break;
    }
}

void AlertRun::speakOnFail(StatusComponent c) {
    if (AlertState::getStatus(c) == SC_Status::LAST_TRY) {
        AlertState::set(c, 15);  // → FAILED
        SpeakRun::speakFail(c);
    }
}