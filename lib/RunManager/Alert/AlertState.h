/**
 * @file AlertState.h
 * @brief Hardware status state storage
 * @version 260206A
 * @date 2026-02-06
 */
#pragma once

#include <stdint.h>

// Component identifiers for boot status tracking
// CRITICAL: Order MUST match health.js FLAGS array!
enum StatusComponent {
    SC_SD = 0,
    SC_WIFI,
    SC_RTC,
    SC_AUDIO,
    SC_DIST,
    SC_LUX,
    SC_SENSOR3,
    SC_NTP,
    SC_WEATHER,
    SC_CALENDAR,
    SC_TTS,
    SC_NAS,
    SC_COUNT  // = 12 (StatusComponent count)
};

// Status interpretation (4-bit value → logical status)
enum class SC_Status : uint8_t { 
    OK,        // 0
    RETRY,     // 2-14
    LAST_TRY,  // 1
    FAILED,    // 15
    ABSENT     // hardware not present per HWconfig.h
};

// Status values for 4-bit fields (legacy, pending removal)
constexpr uint8_t STATUS_OK = 0;
constexpr uint8_t STATUS_NOTOK = 15;  // 0xF

namespace AlertState {
    // ===== NEW API (v4) =====
    uint8_t get(StatusComponent c);
    void setRaw(StatusComponent c, uint8_t value);  // internal, use set() instead
    
    /// @brief Set component status (any integer type, abs + clamp to 0-15)
    template<typename T>
    void set(StatusComponent c, T value) {
        int v = (value < 0) ? -value : value;
        setRaw(c, (v > 15) ? 15 : static_cast<uint8_t>(v));
    }
    
    SC_Status getStatus(StatusComponent c);  // interpreted status
    bool isPresent(StatusComponent c);       // hardware present per HWconfig.h
    void setSdBusy(bool busy);
    bool isSdBusy();
    
    // Legacy - backward compatible
    bool isStatusOK(StatusComponent c);
    void setStatusOK(StatusComponent c, bool status=true);
    uint64_t getBootStatus();
    
    // ===== LEGACY API (backward compatible) =====
    // Status setters - only called by AlertRun::report()
    void setSdStatus(bool status);
    void setWifiStatus(bool status);
    void setRtcStatus(bool status);
    void setNtpStatus(bool status);
    void setDistanceSensorStatus(bool status);
    void setLuxSensorStatus(bool status);
    void setSensor3Status(bool status);
    void setAudioStatus(bool status);
    void setWeatherStatus(bool status);
    void setCalendarStatus(bool status);
    void setTtsStatus(bool status);
    void setNasStatus(bool status);
    void startRuntime();
    
    // Status getters - for Policy and other modules
    bool isSdOk();
    bool isWifiOk();
    bool isRtcOk();
    bool isNtpOk();
    bool isDistanceSensorOk();
    bool isLuxSensorOk();
    bool isSensor3Ok();
    bool isAudioOk();
    bool isWeatherOk();
    bool isCalendarOk();
    bool isTtsOk();
    bool isNasOk();
    bool isBootPhase();
    
    // Gating functions - check prerequisites for specific operations
    bool canPlayHeartbeat();   // SC_AUDIO
    bool canPlayTTS();         // SC_WIFI + SC_AUDIO
    bool canPlayMP3Words();    // SC_SD + SC_AUDIO
    bool canPlayFragment();    // SC_SD + SC_AUDIO + SC_CALENDAR
    bool canFetch();           // SC_WIFI
    
    // Health bits aggregate (for /api/health endpoint)
    // Bit 0:SD 1:WiFi 2:RTC 3:Audio 4:Dist 5:Lux 6:Sensor3 7:NTP 8:Weather 9:Calendar 10:TTS 11:NAS
    uint16_t getHealthBits();
    uint16_t getAbsentBits();  // Bits set for hardware not present per HWconfig
    
    // Initialisatie
    void reset();
}