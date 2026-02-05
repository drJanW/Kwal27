/**
 * @file PlayFragment.h
 * @brief MP3 fragment playback with fade-in/fade-out support
 * @version 260205A
 * @date 2026-02-05
 * 
 * PlayAudioFragment handles playback of MP3 files from SD card subdirectories.
 * Each fragment is played from a specified start position for a given duration,
 * with configurable fade effects using a sine-power curve.
 * 
 * Timers control:
 * - Fade-in steps (curve[0] to curve[N])
 * - Fade-out delay and steps (curve[N] to curve[0])
 * - Playback completion (duration timer)
 * 
 * Never uses loop() return value for completion detection (T4 rule).
 */
#pragma once
#include <Arduino.h>

/**
 * @brief Descriptor for audio fragment playback
 * 
 * Contains all parameters needed to play an MP3 fragment from SD card.
 */
struct AudioFragment {
  uint8_t  dirIndex;    ///< SD card directory (001-200)
  uint8_t  fileIndex;   ///< File within directory (001-101)
  uint8_t  score;       ///< Fragment score for weighted selection
  uint32_t startMs;     ///< Start position in milliseconds (seek target)
  uint32_t durationMs;  ///< Playback duration in milliseconds
  uint16_t fadeMs;      ///< Fade duration (both in and out)
};

/**
 * @brief Namespace for MP3 fragment playback operations
 */
namespace PlayAudioFragment {
  /// Start fragment playback with fade-in
  /// @param fragment Fully populated fragment descriptor
  /// @return true if playback started successfully
  bool start(const AudioFragment& fragment);
  
  /// Stop playback with optional fade-out
  /// @param fadeOutMs Fade duration; 0xFFFF = use current fade; <=40ms = immediate stop
  void stop(uint16_t fadeOutMs = 0xFFFF);
  
  /// Abort playback immediately without fade
  void abortImmediate();
  
  /// Recalculate and apply gain (call when volume changes)
  void updateGain();
}
