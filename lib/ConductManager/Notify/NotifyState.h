/**
 * @file NotifyState.h
 * @brief Hardware status state storage
 * @version 260104M
 * @date 2026-01-04
 *
 * Stores hardware component status in a single uint64_t with 4-bit fields.
 * Each component uses 4 bits: 0=OK, 1-14=retries remaining, 15=FAILED.
 * 
 * Status values (SC_Status enum):
 *   OK (0)       - Component working normally
 *   RETRY (2-14) - Init in progress, N retries remaining  
 *   LAST_TRY (1) - Final init attempt
 *   FAILED (15)  - Init gave up after all retries
 *   ABSENT       - Hardware not present per HWconfig.h *_PRESENT defines
 *
 * Hardware presence: Components marked absent in HWconfig.h (*_PRESENT=false)
 * are skipped during init, show '—' in health displays, and never trigger
 * error flashes or reminders. Use isPresent() to check before error handling.
 *
 * API: getAbsentBits() returns bitmask of absent hardware for /api/health.
 */

#pragma once

#include <stdint.h>

// Component identifiers for boot status tracking
// KRITIEK: Volgorde MOET matchen met health.js FLAGS array!
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
    SC_COUNT  // = 11 (StatusComponent count)
};

// Status interpretatie (4-bit waarde → logische status)
enum class SC_Status : uint8_t { 
    OK,        // 0
    RETRY,     // 2-14
    LAST_TRY,  // 1
    FAILED,    // 15
    ABSENT     // hardware not present per HWconfig.h
};

// Status values for 4-bit fields (legacy, te verwijderen)
constexpr uint8_t STATUS_OK = 0;
constexpr uint8_t STATUS_NOTOK = 15;  // 0xF

namespace NotifyState {
    // ===== NEW API (v4) =====
    uint8_t get(StatusComponent c);
    void set(StatusComponent c, uint8_t value);
    SC_Status getStatus(StatusComponent c);  // geïnterpreteerd
    bool isPresent(StatusComponent c);       // hardware present per HWconfig.h
    
    // Legacy - backward compatible
    bool isStatusOK(StatusComponent c);
    void setStatusOK(StatusComponent c, bool isOK=true);
    uint64_t getBootStatus();
    
    // ===== LEGACY API (backward compatible) =====
    // Status setters - alleen aangeroepen door NotifyConduct::report()
    void setSdStatus(bool isOK);
    void setWifiStatus(bool isOK);
    void setRtcStatus(bool isOK);
    void setNtpStatus(bool isOK);
    void setDistanceSensorStatus(bool isOK);
    void setLuxSensorStatus(bool isOK);
    void setSensor3Status(bool isOK);
    void setAudioStatus(bool isOK);
    void setWeatherStatus(bool isOK);
    void setCalendarStatus(bool isOK);
    void setTtsStatus(bool isOK);
    void startRuntime();
    
    // Status getters - voor Policy en andere modules
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
    bool isBootPhase();
    
    // Gating functions - check prerequisites for specific operations
    bool canPlayHeartbeat();   // SC_AUDIO
    bool canPlayTTS();         // SC_WIFI + SC_AUDIO
    bool canPlayMP3Words();    // SC_SD + SC_AUDIO
    bool canPlayFragment();    // SC_SD + SC_AUDIO + SC_CALENDAR
    bool canFetch();           // SC_WIFI
    
    // Health bits aggregate (for /api/health endpoint)
    // Bit 0:SD 1:WiFi 2:RTC 3:Audio 4:Dist 5:Lux 6:Sensor3 7:NTP 8:Weather 9:Calendar 10:TTS
    uint16_t getHealthBits();
    uint16_t getAbsentBits();  // Bits set for hardware not present per HWconfig
    
    // Initialisatie
    void reset();
}
