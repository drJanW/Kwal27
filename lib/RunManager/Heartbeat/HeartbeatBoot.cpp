/**
 * @file HeartbeatBoot.cpp
 * @brief Heartbeat LED one-time initialization implementation
 * @version 260131A
 * @date 2026-01-31
 */
#include "HeartbeatBoot.h"

#include "Globals.h"

HeartbeatBoot heartbeatBoot;

void HeartbeatBoot::plan() {
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
    PL("[Run][Plan] Heartbeat LED initialized");
}
