/**
 * @file SDRun.h
 * @brief SD card state management with periodic health check
 * @version 260218C
 * @date 2026-02-18
 */
#pragma once

#include <Arduino.h>

class SDRun {
public:
    void plan();
    static void cb_checkSdHealth();
};
