/**
 * @file SystemBoot.cpp
 * @brief System-level boot stages implementation
 * @version 251231E
 * @date 2025-12-31
 *//@@ 2026-01-26 - more extenive commenting on staged boot
 *//@@ how many stages? only pre-boot & boot??
 * Implements staged boot sequence:
 * - Stage 0: Serial, RNG, OTA check
 * - Stage 1: I2C init, component probing via ConductManager
 */

#include <Arduino.h>

#include "SystemBoot.h"
#include "Globals.h"
#include "OTAManager.h"
#include "ConductManager.h"
#include <Wire.h>

bool systemBootStage0() {
    Serial.begin(115200);
    while (!Serial) { delay(10); }
    delay(50);         // Let hardware RNG settle BEFORE seeding
    bootRandomSeed();  // Seed RNG after hardware is ready
    PF("\n[Stage 0] Version %s\n", FIRMWARE_VERSION);
    otaBootHandler();  // Check if OTA mode was requested
    return true;       // Stage 0 complete
}

void systemBootStage1() {
    PL("[Stage 1] Component probing");
    Wire.begin(I2C_SDA, I2C_SCL);
    //@@ no check for wire success, fail
    ConductManager::begin();
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
