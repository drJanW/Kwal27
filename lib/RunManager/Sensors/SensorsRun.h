/**
 * @file SensorsRun.h
 * @brief Sensor data update state management
 * @version 260205A
 * @date 2026-02-05
 */
#pragma once

#include <Arduino.h>

class SensorsRun {
public:
    void plan();
    static void readRtcTemperature();
};
