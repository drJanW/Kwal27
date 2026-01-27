/**
 * @file HeartbeatConduct.cpp
 * @brief Heartbeat LED state management implementation
 * @version 251231E
 * @date 2025-12-31
 *
 * Implements heartbeat LED orchestration: manages on/off timing, adjusts
 * blink pattern based on hardware failure flags, and schedules LED toggle
 * timers through TimerManager.
 */

#include "HeartbeatConduct.h"

#include "Globals.h"
#include "TimerManager.h"
#include "HeartbeatPolicy.h"
#include "ContextFlags.h"
#include "ContextStatus.h"

namespace {

#if HEARTBEAT_DEBUG
#define HB_LOG(...) PF(__VA_ARGS__)
#else
#define HB_LOG(...) do {} while (0)
#endif

uint32_t onMs = 500;
uint32_t offMs = 500;
bool ledState = false;

void updateFailurePattern() {
    const bool anyFail = ContextFlags::getHardwareFailBits() != 0;
    if (anyFail) {
        // Failure pattern: 0.5s aan, 3s uit
        onMs = 500;
        offMs = 3000;
    } else {
        // Normaal: 0.5s aan, 0.5s uit
        onMs = 500;
        offMs = 500;
    }
}

void cb_heartbeat() {
    ledState = !ledState;
    digitalWrite(LED_PIN, ledState ? HIGH : LOW);
    
    updateFailurePattern();
    uint32_t nextMs = ledState ? onMs : offMs;
    TimerManager::instance().restart(nextMs, 1, cb_heartbeat);
}

} // namespace

HeartbeatConduct heartbeatConduct;

void HeartbeatConduct::plan() {
    HeartbeatPolicy::configure();
    updateFailurePattern();
    TimerManager::instance().restart(onMs, 1, cb_heartbeat);
    HB_LOG("[HeartbeatConduct] Started asymmetric heartbeat\n");
}

void HeartbeatConduct::setRate(uint32_t intervalMs) {
    // Legacy - nu asymmetrisch, deze functie doet niets meer
}

uint32_t HeartbeatConduct::currentRate() const {
    return onMs;
}

void HeartbeatConduct::signalError() {
    // Niet meer nodig - failure pattern is altijd actief
}
