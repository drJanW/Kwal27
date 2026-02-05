/**
 * @file SystemBoot.h
 * @brief System-level boot stages
 * @version 260205A
 * @date 2026-02-05
 */
#pragma once

/**
 * @brief Stage 0: Hardware primitives before any controller
 * Serial, RNG, OTA check. No status64 access yet.
 * @return true if stage 0 complete, false if fatal failure
 */
bool systemBootStage0();

/**
 * @brief Stage 1: Component probing and timer setup
 * Initializes I2C, probes all components, populates status64.
 * Stage 2 triggers automatically via OK reports.
 */
bool systemBootStage1();

/**
 * @brief Fatal halt with frantic LED blink - never returns
 * Exception to no-delay architecture: system is dead anyway.
 */
[[noreturn]] void haltBlink();
