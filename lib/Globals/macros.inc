/**
 * @file macros.inc
 * @brief Logging macros and SERIAL_ENABLED switch
 * @version 251231E
 * @date 2025-12-31
 *
 * This file defines the logging system macros used throughout the project.
 * It provides a SERIAL_ENABLED master switch for production builds, log
 * level filtering (ERROR/WARN/INFO/DEBUG), boot spam control, and
 * convenience macros for serial output (PF, PL, LOG_*). When SERIAL_ENABLED
 * is set to 0, serial output is disabled but LogBuffer is still filled
 * for WiFi log access. Include this file in any source that needs logging.
 */

#include "LogBuffer.h"

// ─────────────────────────────────────────────────────────────
// Serial output master switch
// Set to 0 for production firmware with no serial output
// ─────────────────────────────────────────────────────────────
#ifndef SERIAL_ENABLED
#define SERIAL_ENABLED 1
#endif

// ─────────────────────────────────────────────────────────────
// Boot spam toggle - verbose [Run][Plan] messages during boot
// Set to 0 to reduce boot output while keeping errors/warnings
// ─────────────────────────────────────────────────────────────
#ifndef LOG_BOOT_SPAM
#define LOG_BOOT_SPAM 1
#endif

#define LOG_LEVEL_ERROR 0
#define LOG_LEVEL_WARN  1
#define LOG_LEVEL_INFO  2
#define LOG_LEVEL_DEBUG 3
#define LOG_LEVEL_NONE (-1)

#ifndef LOG_LEVEL
#define LOG_LEVEL LOG_LEVEL_NONE
#endif

#ifndef LOCAL_LOG_LEVEL
#define LOCAL_LOG_LEVEL LOG_LEVEL
#endif

#ifndef LOG_HEARTBEAT
#define LOG_HEARTBEAT 0
#endif

#ifndef LOG_TIMESTAMP
#define LOG_TIMESTAMP 1
#endif

#if SERIAL_ENABLED

#define LOG_LEVEL_ACTIVE(lvl) ((lvl) <= LOCAL_LOG_LEVEL)

#define LOG_PRINT(level, ...)   do { if (LOG_LEVEL_ACTIVE(level)) Serial.printf(__VA_ARGS__); } while (0)
#define LOG_PRINTLN(level, msg) do { if (LOG_LEVEL_ACTIVE(level)) Serial.println(msg); } while (0)
#define LOG_WRITE(level, ch)    do { if (LOG_LEVEL_ACTIVE(level)) Serial.write(ch); } while (0)

#define LOG_ERROR(...) LOG_PRINT(LOG_LEVEL_ERROR, __VA_ARGS__)
#define LOG_WARN(...)  LOG_PRINT(LOG_LEVEL_WARN,  __VA_ARGS__)
#define LOG_INFO(...)  LOG_PRINT(LOG_LEVEL_INFO,  __VA_ARGS__)
#define LOG_DEBUG(...) LOG_PRINT(LOG_LEVEL_DEBUG, __VA_ARGS__)

#define LOG_ERROR_LN(msg) LOG_PRINTLN(LOG_LEVEL_ERROR, msg)
#define LOG_WARN_LN(msg)  LOG_PRINTLN(LOG_LEVEL_WARN,  msg)
#define LOG_INFO_LN(msg)  LOG_PRINTLN(LOG_LEVEL_INFO,  msg)
#define LOG_DEBUG_LN(msg) LOG_PRINTLN(LOG_LEVEL_DEBUG, msg)

#define PP(x) do { if (LOG_LEVEL_ACTIVE(LOG_LEVEL_INFO)) Serial.print(x); } while (0)
#define PL(x) do { \
    if (LOG_TIMESTAMP) { \
        char __ts[12]; \
        if (LogBuffer::getTimestamp(__ts, sizeof(__ts)) > 0) { \
            LogBuffer::appendf("%s", __ts); \
            if (LOG_LEVEL_ACTIVE(LOG_LEVEL_INFO)) Serial.print(__ts); \
        } \
    } \
    LogBuffer::appendLine(x); \
    LOG_INFO_LN(x); \
} while(0)
#define PF(...) do { \
    if (LOG_TIMESTAMP) { \
        char __ts[12]; \
        if (LogBuffer::getTimestamp(__ts, sizeof(__ts)) > 0) { \
            LogBuffer::appendf("%s", __ts); \
            if (LOG_LEVEL_ACTIVE(LOG_LEVEL_INFO)) Serial.print(__ts); \
        } \
    } \
    LogBuffer::appendf(__VA_ARGS__); \
    LOG_INFO(__VA_ARGS__); \
} while(0)

// Boot spam macros - for verbose boot messages
#define PL_BOOT(x) do { if (LOG_BOOT_SPAM) PL(x); } while(0)
#define PF_BOOT(...) do { if (LOG_BOOT_SPAM) PF(__VA_ARGS__); } while(0)

#else // SERIAL_ENABLED == 0

#define LOG_LEVEL_ACTIVE(lvl) (false)
#define LOG_PRINT(level, ...)
#define LOG_PRINTLN(level, msg)
#define LOG_WRITE(level, ch)
#define LOG_ERROR(...)
#define LOG_WARN(...)
#define LOG_INFO(...)
#define LOG_DEBUG(...)
#define LOG_ERROR_LN(msg)
#define LOG_WARN_LN(msg)
#define LOG_INFO_LN(msg)
#define LOG_DEBUG_LN(msg)
#define PP(x)
#define PL(x) do { \
    if (LOG_TIMESTAMP) { \
        char __ts[12]; \
        if (LogBuffer::getTimestamp(__ts, sizeof(__ts)) > 0) LogBuffer::appendf("%s", __ts); \
    } \
    LogBuffer::appendLine(x); \
} while(0)
#define PF(...) do { \
    if (LOG_TIMESTAMP) { \
        char __ts[12]; \
        if (LogBuffer::getTimestamp(__ts, sizeof(__ts)) > 0) LogBuffer::appendf("%s", __ts); \
    } \
    LogBuffer::appendf(__VA_ARGS__); \
} while(0)
#define PL_BOOT(x)
#define PF_BOOT(...)

#endif // SERIAL_ENABLED

#define HALT_RESTART_DELAY_MS (5 * 60 * 1000)  // 5 minutes

#define HALT(msg) do { \
    Serial.printf("[FATAL] %s\n", msg); \
    Serial.println("[FATAL] Restarting in 5 minutes..."); \
    delay(HALT_RESTART_DELAY_MS); \
    ESP.restart(); \
} while (0)

static inline void LOG_HEARTBEAT_TICK(char c = '.') {
#if LOG_HEARTBEAT
    Serial.write(c);
#endif
}

/*
#define DBG_TIMED_BLOCK(label, code) { \
    uint32_t __t0 = millis(); \
    code \
    uint32_t __dt = millis() - __t0; \
    if (__dt > 100) LOG_WARN("[SLOW] %s: %lu ms\n", label, __dt); \
}
*/