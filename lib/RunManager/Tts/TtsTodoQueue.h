/**
 * @file TtsTodoQueue.h
 * @brief Self-consuming TTS render-queue from SD card
 * @version 260604C
 * @date 2026-06-04
 *
 * Reads /tts_todo.csv at boot, renders each line via VoiceRSS to a
 * caller-specified SD location (/<dir>/<file>.mp3), then atomically
 * rewrites the CSV without the processed line. Survives reboots and
 * power-loss (crash recovery via .tmp file).
 *
 * Feature-agnostic: any code path can populate tts_todo.csv to get
 * pre-rendered TTS files. Designed for demo (chapter 1..31) but works
 * for any batch use case.
 *
 * Format per line: lang; voice; tempo; dir; file; tekst
 *   lang  = NL (only allowed value for now)
 *   voice = 0..2 (Lotte/Bram/Daan) or -1 (random)
 *   tempo = -3..+3 or 99 (random)
 *   dir   = 1..200 SD directory number
 *   file  = 1..99
 *   tekst = max 160 chars, no semicolons
 *
 * See docs/pseudo_ttsqueue.md for full spec.
 */
#pragma once
#include <Arduino.h>

namespace TtsTodoQueue {

/// Schedule the boot-time queue scan. Call once from boot sequencer
/// when SD + WiFi are available. Does nothing if the queue file is
/// missing or empty.
void plan();

}  // namespace TtsTodoQueue
