/**
 * @file PlayPCM.h
 * @brief Raw PCM audio playback for sound effects (ping, alerts)
 * @version 260205A
 * @date 2026-02-05
 * 
 * Loads and plays 16-bit mono PCM WAV files from SD card.
 * Used for distance sensor feedback (ping.wav) and alert sounds.
 * 
 * WAV format requirements (enforced by policy):
 * - Sample rate: 22050 Hz
 * - Channels: 1 (mono)
 * - Bits per sample: 16
 * - Format: PCM (no compression)
 */
#pragma once

#include "AudioManager.h"

/**
 * @brief Namespace for PCM clip loading and playback
 */
namespace PlayPCM {

using PCM = AudioManager::PCMClipDesc;

/// Load a 16-bit mono PCM WAV file into memory cache
/// @param path SD card path (e.g., "/ping.wav")
/// @return Pointer to cached clip, nullptr on failure
PCM* loadFromSD(const char* path);

/// Play cached PCM clip at specified volume
/// @param clip Pointer to loaded clip (from loadFromSD)
/// @param volume Playback volume (0.0 to 1.0)
/// @param durationMs Maximum playback duration; 0 = play full clip
/// @return true if playback started
bool play(const PCM* clip, float volume, uint16_t durationMs = 0);

}
