/**
 * @file SensorsRun.h
 * @brief Sensor data update state management
 * @version 260202A
 $12026-02-10
 */
#pragma once

#include <Arduino.h>

class SensorsRun {
public:
    void plan();
    static void readRtcTemperature();
};
