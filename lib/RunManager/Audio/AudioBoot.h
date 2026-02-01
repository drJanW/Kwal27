/**
 * @file AudioBoot.h
 * @brief Audio subsystem one-time initialization
 * @version 251231E
 * @date 2025-12-31
 *
 * Handles audio subsystem initialization during boot. Configures the audio
 * hardware, initializes AudioManager, and loads audio shift configurations
 * from SD card. Part of the Boot→Plan→Policy→Run pattern.
 */

#pragma once

#include <Arduino.h>

class AudioBoot {
public:
    void plan();
};
