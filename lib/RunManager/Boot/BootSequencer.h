/**
 * @file BootSequencer.h
 * @brief Declarative boot sequence coordinator
 * @version 260305B
 * @date 2026-03-05
 *
 * Replaces the ad-hoc boolean-flag boot chain with a manifest-driven
 * sequencer. Each boot step declares its dependencies (requiresAll) and
 * what it provides. The sequencer evaluates the manifest, starts steps
 * as deps become available, and fires a single verdict when all steps
 * are terminal (DONE/FAILED) or on boot timeout.
 *
 * Usage:
 *   systemBootStage0/1 → BootSequencer::begin(preGranted)
 *   Async modules      → BootSequencer::grant(Cap::XXX)
 *   Failed modules     → BootSequencer::fail(Cap::XXX)
 *   Query              → BootSequencer::has(Cap::XXX)
 */
#pragma once
#include <stdint.h>
#include "Cap.h"

/// Result returned by a step's init() function
enum class StepResult : uint8_t {
    DONE,      ///< Step completed synchronously — grant capability
    PENDING,   ///< Step started async work — will call grant/fail later
    FAILED     ///< Step cannot complete — cascade dep failure
};

/// Internal state of a step in the manifest
enum class StepState : uint8_t {
    WAITING,   ///< Dependencies not yet met
    RUNNING,   ///< init() returned PENDING — waiting for grant/fail
    DONE,      ///< Capability granted
    FAILED     ///< Cannot complete (own failure or dep cascade)
};

/// A single boot step in the manifest
struct BootStep {
    const char*    name;         ///< Human-readable label for logging
    uint16_t       requiresAll;  ///< All these caps must be granted before init()
    uint16_t       provides;     ///< Caps granted when step succeeds (0 = fire-and-forget)
    StepResult   (*init)();      ///< Start the work — returns sync result
};

/// Central boot sequencer — manages the manifest and fires the verdict
class BootSequencer {
public:
    /// Start the sequencer with pre-granted capabilities from Stage0/1
    static void begin(uint16_t preGranted);

    /// Grant capability (called by async modules when they complete)
    static void grant(uint16_t caps);

    /// Fail capability (called when a step exhausts retries)
    static void fail(uint16_t caps);

    /// Query: does the system have this capability?
    static bool has(uint16_t caps);

    /// Query: is the boot sequence complete (verdict fired)?
    static bool isBootDone();

    /// Current granted bitmask (for logging/status)
    static uint16_t granted();

    /// Human-readable cap name for a single bit
    static const char* capName(uint16_t singleCap);

private:
    static void evaluate();
    static void cascadeFailure(uint16_t failedCaps);
    static void checkBootDone();
    static void enterSteadyState();
    static void cb_evaluateSteps();
    static void cb_expireBoot();

    static constexpr uint8_t MAX_STEPS = 16;

    static uint16_t    granted_;
    static uint16_t    failed_;
    static StepState   states_[MAX_STEPS];
    static uint8_t     stepCount_;
    static bool        verdictDone_;
};
