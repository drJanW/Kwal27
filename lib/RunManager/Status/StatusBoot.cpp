/**
 * @file StatusBoot.cpp
 * @brief Status display one-time initialization implementation
 * @version 260204A
 * @date 2026-02-04
 */
#include "StatusBoot.h"
#include "Globals.h"
#include "StatusPolicy.h"
#include "PRTClock.h"
#include "Alert/AlertRun.h"
#include "ContextController.h"

StatusBoot statusBoot;

void cb_timeDisplay() {
    // Consolidated into cb_readRtcTemperature in SensorsRun
}

void StatusBoot::plan() {
    PL_BOOT("[StatusBoot] sequencing");
    StatusPolicy::configure();
    AlertRun::plan();
}
