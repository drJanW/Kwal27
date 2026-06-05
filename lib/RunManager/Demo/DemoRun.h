/**
 * @file DemoRun.h
 * @brief 31-chapter demonstration orchestrator (Kwal2 v1.0 demo)
 * @version 260604E
 * @date 2026-06-04
 *
 * Plays the full Kwal demo: per chapter TTS narration (from /<demoDir>/),
 * then a music/sfx fragment from the same dir, while driving pattern+colors.
 *
 * Web API:
 *   GET /api/demomode             — start demo (chapter 0)
 *   GET /api/demostop             — stop demo, restore normal orchestration
 *
 * While `Globals::demoActive == true` the normal cb_playFragment / cb_sayTime
 * callbacks no-op (guarded in RunManager). Calendar/context selection is
 * also suppressed via the same flag.
 */
#pragma once
#include <Arduino.h>

namespace DemoRun {

void start();      // Begin chapter 0
void stop();       // Cancel timers, restore normal mode
bool isActive();   // Mirror of Globals::demoActive
uint8_t chapterCount();

}  // namespace DemoRun
