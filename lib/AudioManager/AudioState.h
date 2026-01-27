/**
 * @file AudioState.h
 * @brief Shared audio state interface between playback modules
 * @version 251231E
 * @date 2025-12-31
 *
 * Declares functions for accessing and modifying shared audio state.
 * Provides thread-safe getters/setters for playback status and gain levels.
 * Used by AudioManager, PlayFragment, PlaySentence, and PlayPCM modules.
 * Enables coordination between different audio playback components.
 */

#pragma once

#include <Arduino.h>

// Check if TTS is currently active
bool isTtsActive();

float getVolumeWebShift();
void setVolumeWebShift(float value);
int getAudioSliderPct();  // Current slider position as percentage
void setAudioLevelRaw(int16_t value);
int16_t getAudioLevelRaw();
float getVolumeShiftedHi();
void setVolumeShiftedHi(float value);
bool isAudioBusy();
void setAudioBusy(bool value);
bool getCurrentDirFile(uint8_t& dir, uint8_t& file, uint8_t& score);
void setCurrentDirFile(uint8_t dir, uint8_t file, uint8_t score);
bool isFragmentPlaying();
void setFragmentPlaying(bool value);
bool isSentencePlaying();
void setSentencePlaying(bool value);
void setTtsActive(bool value);
void setWordPlaying(bool value);
void setCurrentWordId(int32_t value);
