/**
 * @file ContextController.h
 * @brief Central context coordination and TodayContext management interface
 * @version 251231E
 * @date 2025-12-31
 *
 * Header file for the central context coordination system.
 * Defines the ContextController namespace with TimeContext structure for time tracking,
 * WebCmd enumeration for web interface commands, and functions for managing
 * the application's overall context state.
 */

#pragma once
#include <Arduino.h>

namespace ContextController {
  enum class WebCmd : uint8_t { None = 0, NextTrack, DeleteFile, ApplyVote, BanFile };

  struct TimeContext {
    uint8_t hour{0};
    uint8_t minute{0};
    uint8_t second{0};
    uint16_t year{2000};
    uint8_t month{1};
    uint8_t day{1};
    uint8_t dayOfWeek{0};
    uint16_t dayOfYear{1};
    uint8_t sunriseHour{0};
    uint8_t sunriseMinute{0};
    uint8_t sunsetHour{0};
    uint8_t sunsetMinute{0};
    float moonPhase{0.0f};
    float weatherMinC{0.0f};
    float weatherMaxC{0.0f};
    bool hasWeather{false};
    bool synced{false};
  };

  const TimeContext& time();
  void refreshTimeRead();
  void updateWeather(float minC, float maxC);
  void clearWeather();
  void begin();  // Start periodic tick timer
}

// Webthread → post opdracht
// Returns true if command was executed immediately, false if queued for later
bool ContextController_post(ContextController::WebCmd cmd, uint8_t dir = 0, uint8_t file = 0, int8_t delta = 0);