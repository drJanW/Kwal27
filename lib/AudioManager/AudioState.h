/**
 * @file AudioState.h
 * @brief Thread-safe audio state accessors shared between playback modules
 * @version 260212C
 * @date 2026-02-12
 * 
 * Provides atomic getters/setters for audio state shared across modules:
 * - Volume levels (shiftedHi, webMultiplier)
 * - Playback status (fragment, sentence, TTS, PCM)
 * - Current track info (dir, file, score)
 * - Audio meter level
 * 
 * All functions use relaxed memory ordering for cross-core ESP32 safety.
 */
#pragma once

#include <Arduino.h>

/// Check if TTS (Text-to-Speech) is currently active
bool isTtsActive();

/// Get web UI volume multiplier (can be >1.0)
float getVolumeWebMultiplier();

/// Set web UI volume multiplier
void setVolumeWebMultiplier(float value);

/// Get current volume as slider percentage (0-100)
int getAudioSliderPct();

/// Set raw audio level for VU meter display
void setAudioLevelRaw(int16_t value);

/// Get raw audio level for VU meter display
int16_t getAudioLevelRaw();

/// Get volume Hi boundary after shifts applied
float getVolumeShiftedHi();

/// Set volume Hi boundary after shifts applied
void setVolumeShiftedHi(float value);

/// Check if any audio playback is active
bool isAudioBusy();

/// Set audio busy flag
void setAudioBusy(bool value);

/// Get current playing fragment info
/// @param[out] dir Directory index
/// @param[out] file File index  
/// @param[out] score Fragment score
/// @return true if valid fragment is playing
bool getCurrentDirFile(uint8_t& dir, uint8_t& file, uint8_t& score);

/// Set current playing fragment info
void setCurrentDirFile(uint8_t dir, uint8_t file, uint8_t score);

/// Check if MP3 fragment is playing
bool isFragmentPlaying();

/// Set fragment playing flag
void setFragmentPlaying(bool value);

/// Check if TTS sentence is playing
bool isSentencePlaying();

/// Set sentence playing flag
void setSentencePlaying(bool value);

/// Set TTS active flag
void setTtsActive(bool value);

/// Set word playing flag (during sentence playback)
void setWordPlaying(bool value);

/// Set current word ID being spoken
void setCurrentWordId(int32_t value);
