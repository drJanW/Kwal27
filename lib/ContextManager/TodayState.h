/**
 * @file TodayState.h
 * @brief Today's state management interface
 * @version 251231E
 * @date 2025-12-31
 *
 * Declares functions for initializing and accessing today's state.
 * Provides interface for loading TodayState structure with current
 * day's colors, patterns, audio settings, and theme box configuration.
 * Entry point for the module's daily state management.
 */

#pragma once

#include <Arduino.h>
#include <FS.h>

#include "CalendarSelector.h"
#include "ThemeBoxTable.h"
#include "TodayModels.h"

bool InitTodayState(fs::FS& sd, const char* rootPath = "/");
bool TodayStateReady();
bool LoadTodayState(TodayState& state);
const ThemeBox* FindThemeBox(uint8_t id);
const ThemeBox* GetDefaultThemeBox();