/**
 * @file SystemBoot.h
 * @brief System-level boot stages
 * @version 251231E
 * @date 2025-12-31
 *
 * Handles staged boot sequence:
 * - Stage 0: Serial, RNG, OTA check (pre-architecture)
 * - Stage 1: Component probing, timer setup, status64 population
 * - Stage 2: Per-component when prerequisites met (via OK reports)
 */

#ifndef SYSTEMBOOT_H
#define SYSTEMBOOT_H

/**
 * @brief Stage 0: Hardware primitives before any manager
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

#endif // SYSTEMBOOT_H
