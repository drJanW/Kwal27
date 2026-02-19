/**
 * @file SystemBoot.cpp
 * @brief System-level boot stages implementation
 * @version 260219E
 * @date 2026-02-19
 */
#include <Arduino.h>
#include "SystemBoot.h"
#include "Globals.h"
#include "HWconfig.h"
#include "RunManager.h"
#include "RTCController.h"
#include "PRTClock.h"
#include <Preferences.h>
#include <Wire.h>

// Serial init timeout (ms) for headless boot scenarios
constexpr uint32_t SERIAL_TIMEOUT_MS = 2000;

bool systemBootStage0() {
    Serial.begin(SERIAL_BAUD);
    uint32_t serialStart = millis();
    while (!Serial && (millis() - serialStart < SERIAL_TIMEOUT_MS)) { 
        delay(10); 
    }
    PF(" Firmware %s\n", FIRMWARE_VERSION_CODE);
    delay(50);         // Let hardware RNG settle BEFORE seeding
    bootRandomSeed();  // Seed RNG after hardware is ready
    Globals::fillFadeCurve();  // Precompute shared sine² fade curve
    // deviceName not yet known (config.txt loaded after SD init in Globals::begin())

    // Clear stale NVS OTA mode flags (legacy ArduinoOTA remnant)
    Preferences prefs;
    prefs.begin("ota", false);
    const uint8_t otaMode = prefs.getUChar("mode", 0);
    if (otaMode != 0) {
        prefs.putUChar("mode", 0);
        PF("[OTA] cleared stale NVS mode=%u\n", otaMode);
    }
    prefs.end();

    return true;       // Stage 0 complete
}

bool systemBootStage1() {
    // Initialize I2C bus
    bool wireOk = Wire.begin(I2C_SDA, I2C_SCL);
    if (wireOk) {
        Wire.setClock(I2C_CLOCK_HZ);
        hwStatus |= HW_I2C;
        PL(" I2C: OK");

        // Pre-boot exception: read RTC before RunManager starts
        // so time is known for all subsequent boot stages.
        // Calls controller directly (normal Boot->Policy->Controller
        // stack not available yet).
        if (Globals::rtcPresent) {
            RTCController::begin();
            if (RTCController::readInto(prtClock)) {
                prtClock.setTimeFetched(true);
                if (RTCController::wasPowerLost()) {
                    PL(" RTC: power lost");
                } else {
                    PL(" RTC: OK");
                }
            } else {
                PL(" RTC: FAIL");
            }
        }
    } else {
        PL(" I2C: FAIL");
    }
    
    RunManager::begin();
    return wireOk;
}

void haltBlink() {
    pinMode(LED_BUILTIN, OUTPUT);
    while (1) {
        digitalWrite(LED_BUILTIN, HIGH);
        delay(HALT_BLINK_MS);
        digitalWrite(LED_BUILTIN, LOW);
        delay(HALT_BLINK_MS);
    }
}
