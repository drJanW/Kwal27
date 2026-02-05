/**
 * @file VL53L1X.h
 * @brief Time-of-flight distance sensor driver interface
 * @version 260205A
 * @date 2026-02-05
 */
#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <SparkFun_VL53L1X.h>

#include "Globals.h"

#ifndef VL53L1X_I2C_ADDR
#define VL53L1X_I2C_ADDR 0x29
#endif

/// Initialize sensor with optional parameters. Wire.begin() is called elsewhere.
bool VL53L1X_begin(uint8_t address = VL53L1X_I2C_ADDR,
                   TwoWire &bus = Wire,
                   uint16_t timingBudgetMs = 50,   // 20..100+
                   bool longRange = false);         // false=Short, true=Long

/// SensorController reads via this function; returns NAN if no new sample.
float readVL53L1X();
