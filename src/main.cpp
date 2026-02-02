/**
 * @file main.cpp
 * @brief Kwal ESP32 Firmware - Main entry point
 * @version 260128A for this file, see globals.h for project version
 * @date 2026-01-28
 *
 * This is the main entry point for the Kwal ambient light and audio sculpture.
 * The firmware runs on ESP32-S3 and coordinates:
 * - LED light shows (patterns, colors, brightness based on ambient light)
 * - Audio fragment playback from SD card (context-aware selection)
 * - Time-based behavior via calendar CSV files
 * - Web interface for configuration and control
 * - OTA firmware updates
 *
 * Architecture:
 * - RunManager: Central run coordinator using Boot→Plan→Policy→Run pattern
 * - TimerManager: Non-blocking timer system (no millis() or delay())
 * - Controllers: AudioManager, LightController, SensorController, SDController, etc.
 *
 * Boot Stages:
 * - Stage 0: Hardware primitives (Serial, RNG, OTA) - SystemBoot
 * - Stage 1: Component probing → status64
 * - Stage 2: Per-component when prerequisites met → actions enabled
 *
 * @author Jan Wilms
 * @author hardware Frank Vermeij
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