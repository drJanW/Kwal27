/**
 * @file HeartbeatBoot.cpp
 * @brief Heartbeat LED one-time initialization implementation
 * @version 260131A
 $12026-02-05
 */
#include "HeartbeatBoot.h"

#include "Globals.h"

HeartbeatBoot heartbeatBoot;

void HeartbeatBoot::plan() {
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
    PL_BOOT("[HeartbeatBoot] LED ready");
}
