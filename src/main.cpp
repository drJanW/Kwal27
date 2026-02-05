/**
 * @file main.cpp
 * @brief Kwal ESP32 Firmware - Main entry point
 * @version 260202A
 * @date 2026-02-02
 */
#include <Arduino.h>

#include "TimerManager.h"
#include "RunManager.h"
#include "System/SystemBoot.h"
#include "Globals.h"

/**
 * @brief Arduino setup - runs once at boot
 * Delegates to staged boot system for proper initialization sequence.
 */
void setup()
{
    // Stage 0: Hardware primitives
    if (!systemBootStage0())
    {
        PL("[Main] Stage 0 FAILED - halting");
        haltBlink();
    }
    // Stage 1: Component probing (Stage 2 via OK reports)
    if (!systemBootStage1()) {
        PL("[Main] Stage 1 incomplete - degraded state");
    }
}

/**
 * @brief Arduino main loop - runs continuously
 * Updates the timer system and RunManager each iteration.
 * All timing is handled by TimerManager callbacks, not by delays.
 */
void loop()
{
    timers.update();
    RunManager::update();
}