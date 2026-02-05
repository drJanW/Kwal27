/**
 * @file SystemBoot.cpp
 * @brief System-level boot stages implementation
 * @version 260201A
 * @date 2026-02-01
 */
#include <Arduino.h>
#include "SystemBoot.h"
#include "Globals.h"
#include "HWconfig.h"
#include "OTAController.h"
#include "RunManager.h"
#include <Wire.h>

// Serial init timeout (ms) for headless boot scenarios
constexpr uint32_t SERIAL_TIMEOUT_MS = 2000;

bool systemBootStage0() {
    Serial.begin(SERIAL_BAUD);
    uint32_t serialStart = millis();
    while (!Serial && (millis() - serialStart < SERIAL_TIMEOUT_MS)) { 
        delay(10); 
    }
    delay(50);         // Let hardware RNG settle BEFORE seeding
    bootRandomSeed();  // Seed RNG after hardware is ready
    PF("\n[Stage 0] Version %s\n", FIRMWARE_VERSION);
    otaBootHandler();  // Check if OTA mode was requested
    return true;       // Stage 0 complete
}

bool systemBootStage1() {
    PL("[Stage 1] Component probing");
    
    // Initialize I2C bus
    bool wireOk = Wire.begin(I2C_SDA, I2C_SCL);
    if (wireOk) {
        Wire.setClock(I2C_CLOCK_HZ);
        hwStatus |= HW_I2C;
        PL("  I2C: OK");
    } else {
        PL("  I2C: FAIL");
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
