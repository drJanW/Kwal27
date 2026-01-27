/**
 * @file VL53L1X.h
 * @brief Time-of-flight distance sensor driver interface
 * @version 251231E
 * @date 2025-12-31
 *
 * Header file for the VL53L1X time-of-flight distance sensor driver.
 * Provides initialization and reading functions for the laser ranging sensor.
 * The sensor measures distance in millimeters using time-of-flight technology.
 * Supports configurable I2C address, timing budget, and short/long range modes.
 */

#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <SparkFun_VL53L1X.h>

#include "Globals.h"

#ifndef VL53L1X_I2C_ADDR
#define VL53L1X_I2C_ADDR 0x29
#endif

// Init met optionele parameters. Wire.begin() wordt elders gedaan.
bool VL53L1X_begin(uint8_t address = VL53L1X_I2C_ADDR,
                   TwoWire &bus = Wire,
                   uint16_t timingBudgetMs = 50,   // 20..100+
                   bool longMode = false);          // false=Short, true=Long

// SensorManager leest via deze functie; geeft NAN als geen nieuwe sample.
float readVL53L1X();
