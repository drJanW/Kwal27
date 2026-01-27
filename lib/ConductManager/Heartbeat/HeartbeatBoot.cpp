/**
 * @file HeartbeatBoot.cpp
 * @brief Heartbeat LED one-time initialization implementation
 * @version 251231E
 * @date 2025-12-31
 *
 * Implements heartbeat boot sequence: configures LED pin as output and
 * sets initial state to off.
 */

#include "HeartbeatBoot.h"

#include "Globals.h"

HeartbeatBoot heartbeatBoot;

void HeartbeatBoot::plan() {
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
    PL("[Conduct][Plan] Heartbeat LED initialized");
}
