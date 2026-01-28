/**
 * @file ContextManager.cpp
 * @brief Central context coordination and TodayContext management
 * @version 251231E
 * @date 2025-12-31
 *
 * This file implements the central context coordination system for the application.
 * It manages TodayContext which contains all current state information including
 * time context, web commands, and coordination between various modules.
 * Handles time updates, web command processing, and context state transitions.
 */

#include "ContextManager.h"
#include "Globals.h"
#include "AudioState.h"
#include "TimerManager.h"
#include "AudioManager.h"
#include "PlayFragment.h"
#include "Audio/AudioDirector.h"
#include "SDVoting.h"
#include "PRTClock.h"

static volatile ContextManager::WebCmd pendingCmd = ContextManager::WebCmd::None;
static volatile uint8_t cmdDir = 0, cmdFile = 0;
static volatile int8_t cmdDelta = 0;
static bool nextPending = false;
static ContextManager::TimeContext timeContext{};
static float weatherMinC = 0.0f;
static float weatherMaxC = 0.0f;
static bool weatherValid = false;

namespace {

void updateTimeContext() {
  timeContext.hour = prtClock.getHour();
  timeContext.minute = prtClock.getMinute();
  timeContext.second = prtClock.getSecond();
  timeContext.year = static_cast<uint16_t>(2000 + prtClock.getYear());
  timeContext.month = prtClock.getMonth();
  timeContext.day = prtClock.getDay();
  timeContext.dayOfWeek = prtClock.getDoW();
  timeContext.dayOfYear = prtClock.getDoY();
  timeContext.sunriseHour = prtClock.getSunriseHour();
  timeContext.sunriseMinute = prtClock.getSunriseMinute();
  timeContext.sunsetHour = prtClock.getSunsetHour();
  timeContext.sunsetMinute = prtClock.getSunsetMinute();
  timeContext.moonPhase = prtClock.getMoonPhaseValue();
  timeContext.synced = prtClock.isTimeFetched();

  if (weatherValid) {
    timeContext.hasWeather = true;
    timeContext.weatherMinC = weatherMinC;
    timeContext.weatherMaxC = weatherMaxC;
  } else {
    timeContext.hasWeather = false;
    timeContext.weatherMinC = 0.0f;
    timeContext.weatherMaxC = 0.0f;
  }
}

} // namespace

// 20 ms periodic via TimerManager
static void ctx_tick_cb() {
  updateTimeContext();

  // 1) pak één command
  ContextManager::WebCmd cmd = pendingCmd;
  if (cmd != ContextManager::WebCmd::None) pendingCmd = ContextManager::WebCmd::None;

  // 2) verwerk command
  if (cmd == ContextManager::WebCmd::NextTrack) {
    nextPending = true;
  } else if (cmd == ContextManager::WebCmd::DeleteFile) {
    if (!isAudioBusy() && !isSentencePlaying()) {
      SDVoting::deleteIndexedFile(cmdDir, cmdFile);
    } else {
      pendingCmd = ContextManager::WebCmd::DeleteFile; // retry zodra idle
    }
  } else if (cmd == ContextManager::WebCmd::ApplyVote) {
    SDVoting::applyVote(cmdDir, cmdFile, cmdDelta);
  } else if (cmd == ContextManager::WebCmd::BanFile) {
    if (!isAudioBusy() && !isSentencePlaying()) {
      SDVoting::banFile(cmdDir, cmdFile);
    } else {
      pendingCmd = ContextManager::WebCmd::BanFile; // retry zodra idle
    }
  }

  // 3) NEXT uitvoeren
  if (nextPending) {
    if (isAudioBusy() || isSentencePlaying()) {
      AudioManager::instance().stop();
      return; // volgende tick start nieuwe
    }
    AudioFragment frag{};
    if (AudioDirector::selectRandomFragment(frag)) {
      if (!PlayAudioFragment::start(frag)) {
        PF("[ContextManager] NEXT failed: fragment start rejected\n");
      }
    }
    nextPending = false;
  }
}

bool ContextManager_post(ContextManager::WebCmd cmd, uint8_t dir, uint8_t file, int8_t delta) {
  cmdDir = dir; cmdFile = file; cmdDelta = delta; pendingCmd = cmd;
  
  // Vote executes immediately (doesn't conflict with audio playback)
  if (cmd == ContextManager::WebCmd::ApplyVote) {
    pendingCmd = ContextManager::WebCmd::None;
    SDVoting::applyVote(dir, file, delta);
    return true;
  }
  
  // Ban/Delete: execute immediately if audio is idle, otherwise queue for tick
  if (cmd == ContextManager::WebCmd::BanFile ||
      cmd == ContextManager::WebCmd::DeleteFile) {
    if (!isAudioBusy() && !isSentencePlaying()) {
      pendingCmd = ContextManager::WebCmd::None;
      if (cmd == ContextManager::WebCmd::BanFile) {
        SDVoting::banFile(dir, file);
      } else {
        SDVoting::deleteIndexedFile(dir, file);
      }
      return true;
    }
    return false; // queued for later
  }
  return false; // other commands are always queued
}

void ContextManager::begin() {
  // Start een 20 ms heartbeat die de context events verwerkt.
  timers.cancel(ctx_tick_cb);
  updateTimeContext();
  if (!timers.create(20, 0, ctx_tick_cb)) {
    PF("[ContextManager] Failed to start context tick timer\n");
  }
}

const ContextManager::TimeContext& ContextManager::time() {
  return timeContext;
}

void ContextManager::refreshTimeRead() {
  updateTimeContext();
}

void ContextManager::updateWeather(float minC, float maxC) {
  weatherMinC = minC;
  weatherMaxC = maxC;
  weatherValid = true;
  updateTimeContext();
}

void ContextManager::clearWeather() {
  weatherValid = false;
  updateTimeContext();
}
