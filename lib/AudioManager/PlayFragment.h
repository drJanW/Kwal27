/**
 * @file PlayFragment.h
 * @brief Audio fragment playback interface with fade support
 * @version 251231E
 * @date 2025-12-31
 *
 * Declares the AudioFragment structure and PlayAudioFragment namespace.
 * AudioFragment defines playback parameters: file location, timing, and fade settings.
 * The PlayAudioFragment namespace provides start/stop/abort functions for fragment playback.
 * Supports configurable fade-in/fade-out for smooth audio transitions.
 */

#pragma once
#include <Arduino.h>

struct AudioFragment {
  uint8_t  dirIndex;
  uint8_t  fileIndex;
  uint8_t  score;
  uint32_t startMs;
  uint32_t durationMs;
  uint16_t fadeMs;
};

namespace PlayAudioFragment {
  bool start(const AudioFragment& fragment);
  void stop(uint16_t fadeOutMs = 0xFFFF); // default uses current fade; <=40ms treated as immediate
  void abortImmediate();
  void updateGain();
}
