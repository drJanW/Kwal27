/**
 * @file SensorsRun.h
 * @brief Sensor data update state management
 * @version 260202A
 * @date 2026-02-02
 */
#pragma once

#include <Arduino.h>

class SensorsRun {
public:
    void plan();
    static void readRtcTemperature();
};
