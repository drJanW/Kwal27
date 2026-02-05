/**
 * @file PlaySentence.h
 * @brief TTS sentence playback using word dictionary from SD card
 * @version 260205A
 * @date 2026-02-05
 * 
 * Plays sequences of pre-recorded words from /000/ directory.
 * Words are played sequentially with configurable inter-word pause.
 * Supports both local MP3 words and remote TTS API fallback.
 * 
 * Uses a unified queue (SpeakItem) to manage mixed word/TTS requests.
 */
#pragma once
#include <Arduino.h>

namespace PlaySentence
{

/// Maximum words in a single sentence
constexpr uint8_t MAX_WORDS_PER_SENTENCE = 50;

/// Marker for end of word array
constexpr uint8_t END_OF_SENTENCE = 255;

/// Pause between words (includes fetch latency compensation)
constexpr uint16_t WORD_INTERVAL_MS = 150;

/// Play next word in queue (internal, called by timer callback)
void playWord();

/// Add word array to queue, starts playback if idle
/// @param words Array of MP3 IDs terminated by END_OF_SENTENCE
void addWords(const uint8_t* words);

/// Add TTS sentence to queue (uses VoiceRSS API)
/// @param sentence Text to speak
void addTTS(const char* sentence);

/// Start TTS playback directly (legacy interface)
void startTTS(const String& text);

/// Get scratchpad buffer for building word arrays at runtime
/// @return Pointer to 8-byte scratch buffer
uint8_t* getScratchpad();

/// Process next item from speak queue (called after playback completes)
void speakNext();

/// Update playback state (pump decoder if needed)
void update();

/// Stop all sentence/word playback
void stop();
}
