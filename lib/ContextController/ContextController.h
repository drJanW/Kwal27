/**
 * @file ContextController.h
 * @brief Central context coordination and TodayState management interface
 * @version 260204A
 $12026-02-10
 */
#pragma once
#include <Arduino.h>

namespace ContextController {
  enum class WebCmd : uint8_t { None = 0, NextTrack, DeleteFile, ApplyVote, BanFile };

  struct TimeState {
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
    float rtcTemperatureC{0.0f};
    bool hasRtcTemperature{false};
    bool synced{false};
  };

  const TimeState& time();
  void refreshTimeRead();
  void updateWeather(float minC, float maxC);
  void clearWeather();
  void updateRtcTemperature(float tempC);
  void clearRtcTemperature();
  void begin();  // Start periodic tick timer
}

// Webthread → post opdracht
// Returns true if command was executed immediately, false if queued for later
bool ContextController_post(ContextController::WebCmd cmd, uint8_t dir = 0, uint8_t file = 0, int8_t delta = 0);