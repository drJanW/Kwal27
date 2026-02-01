/**
 * @file TodayContext.h
 * @brief Today's context state management interface
 * @version 251231E
 * @date 2025-12-31
 *
 * Declares functions for initializing and accessing today's context.
 * Provides interface for loading TodayContext structure with current
 * day's colors, patterns, audio settings, and theme box configuration.
 * Entry point for the context module's daily state management.
 */

#pragma once

#include <Arduino.h>
#include <FS.h>

#include "CalendarSelector.h"
#include "ThemeBoxTable.h"
#include "ContextModels.h"

bool InitTodayContext(fs::FS& sd, const char* rootPath = "/");
bool TodayContextReady();
bool LoadTodayContext(TodayContext& ctx);
const ThemeBox* FindThemeBox(uint8_t id);
const ThemeBox* GetDefaultThemeBox();
