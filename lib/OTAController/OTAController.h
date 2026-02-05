/**
 * @file OTAController.h
 * @brief Over-the-air firmware update handling
 * @version 260202A
 * @date 2026-02-02
 */
#pragma once
#include <Arduino.h>

// 0=normal, 1=pending, 2=ota
void otaArm(uint32_t window_s = 300);
bool otaConfirmAndReboot();     // returns false if expired
void otaBootHandler();          // call very early in setup()
