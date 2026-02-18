/**
 * @file FallbackPage.h
 * @brief PROGMEM fallback HTML served when SD card is unavailable
 * @version 260218E
 * @date 2026-02-18
 */
#pragma once

#include <Arduino.h>

// Minimal self-contained HTML page (~2.5 KB) served from flash when SD is absent.
// Features: status overview via /api/health, OTA firmware upload, restart button.
// Defined in FallbackPage.cpp — single copy in flash.
extern const char FALLBACK_HTML[] PROGMEM;
