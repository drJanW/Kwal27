/**
 * @file OTAController.h
 * @brief Over-the-air firmware update handling
 * @version 251231E
 * @date 2025-12-31
 *
 * This header declares the OTA (Over-The-Air) update control functions.
 * The OTA system uses a two-step confirmation process for safety:
 * 1. otaArm() - Arms the OTA mode with a configurable time window (default 5 min)
 * 2. otaConfirmAndReboot() - Confirms and enters OTA mode if still within window
 * The otaBootHandler() must be called early in setup() to check if the device
 * should enter OTA update mode based on NVS-stored state flags.
 */

#pragma once
#include <Arduino.h>

// 0=normal, 1=pending, 2=ota
void otaArm(uint32_t window_s = 300);
bool otaConfirmAndReboot();     // returns false if expired
void otaBootHandler();          // call very early in setup()
