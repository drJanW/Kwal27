/**
 * @file TodayState.h
 * @brief Today's state management interface
 * @version 260202A
 * @date 2026-02-02
 */
#pragma once

#include <Arduino.h>
#include <FS.h>

#include "Calendar.h"
#include "ThemeBoxTable.h"
#include "TodayModels.h"

bool InitTodayState(fs::FS& sd, const char* rootPath = "/");
bool TodayStateReady();
bool LoadTodayState(TodayState& state);
const ThemeBox* FindThemeBox(uint8_t id);
const ThemeBox* GetDefaultThemeBox();
const std::vector<ThemeBox>& GetAllThemeBoxes();