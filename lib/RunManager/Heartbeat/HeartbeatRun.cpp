/**
 * @file HeartbeatRun.cpp
 * @brief Heartbeat LED state management implementation
 * @version 260205A
 * @date 2026-02-05
 */
#include "HeartbeatRun.h"

#include "Globals.h"
#include "TimerManager.h"
#include "HeartbeatPolicy.h"
#include "StatusFlags.h"
#include "StatusBits.h"

namespace {

#if HEARTBEAT_DEBUG
#define HB_LOG(...) PF(__VA_ARGS__)
#else
#define HB_LOG(...) do {} while (0)
#endif

uint32_t onMs = 500;
uint32_t offMs = 500;
bool ledState = false;

/// Update heartbeat pattern based on hardware failure state
void updateFailurePattern() {
    const bool anyFail = StatusFlags::getHardwareFailBits() != 0;
    if (anyFail) {
        // Failure pattern: 0.5s on, 3s off
        onMs = 500;
        offMs = 3000;
    } else {
        // Normal: 0.5s on, 0.5s off
        onMs = 500;
        offMs = 500;
    }
}

void cb_heartbeat() {
    ledState = !ledState;
    digitalWrite(LED_PIN, ledState ? HIGH : LOW);
    
    updateFailurePattern();
    uint32_t nextMs = ledState ? onMs : offMs;
    timers.restart(nextMs, 1, cb_heartbeat);
}

} // namespace

HeartbeatRun heartbeatRun;

void HeartbeatRun::plan() {
    HeartbeatPolicy::configure();
    updateFailurePattern();
    timers.restart(onMs, 1, cb_heartbeat);
    HB_LOG("[HeartbeatRun] Started asymmetric heartbeat\n");
}

void HeartbeatRun::setRate(uint32_t intervalMs) {
    // Legacy - now asymmetric, this function is no-op
}

uint32_t HeartbeatRun::currentRate() const {
    return onMs;
}

void HeartbeatRun::signalError() {
    // No longer needed - failure pattern is always active
}
