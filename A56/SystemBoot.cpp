/**
 * @file SystemBoot.cpp
 * @brief System-level boot stages implementation
 * @version 260128A
 * @date 2026-01-28
 * 
 * Implements staged boot sequence:
 * - Stage 0: Serial, RNG, OTA check
 * - Stage 1: I2C init, component probing via ConductManager
 * 
 * ConductManager::begin() handles finer subsystem orchestration internally.
 */

#include <Arduino.h>

#include "SystemBoot.h"
#include "Globals.h"
#include "HWconfig.h"
#include "OTAManager.h"
#include "ConductManager.h"
#include <Wire.h>

// Serial init timeout (ms) for headless boot scenarios
constexpr uint32_t SERIAL_TIMEOUT_MS = 2000;

bool systemBootStage0() {
    Serial.begin(115200);
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
        Wire.setClock(400000);  // 400kHz Fast Mode
        hwStatus |= HW_I2C;
        PL("  I2C: OK");
    } else {
        PL("  I2C: FAIL");
    }
    
    ConductManager::begin();
    return wireOk;
}

void haltBlink() {
    pinMode(LED_BUILTIN, OUTPUT);
    while (1) {
        digitalWrite(LED_BUILTIN, HIGH);
        delay(150);
        digitalWrite(LED_BUILTIN, LOW);
        delay(150);
    }
}
