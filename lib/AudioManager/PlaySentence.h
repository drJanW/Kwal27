/**
 * @file PlaySentence.h
 * @brief TTS sentence playback interface using word dictionary
 * @version 251231E
 * @date 2025-12-31
 *
 * Declares the PlaySentence namespace for text-to-speech functionality.
 * Provides functions to queue and play sequences of pre-recorded word audio clips.
 * Supports both word-index arrays (addWords) and text string input (startTTS).
 * Manages word timing and playback state for natural-sounding sentence output.
 */

#pragma once
#include <Arduino.h>

namespace PlaySentence
{

// Constanten
constexpr uint8_t MAX_WORDS_PER_SENTENCE = 50;
constexpr uint8_t END_OF_SENTENCE = 255;
constexpr uint16_t WORD_INTERVAL_MS = 150;  // Pauze + timing marge (compensatie voor fetch latency)

// Interne functie (gedeeld: gebruikt door cb_wordTimer callback + direct bij eerste woord)
void playWord();

// Publieke API
void addWords(const uint8_t* words);  // Voegt woorden toe aan FIFO queue, start playback als nodig
void addTTS(const char* sentence);    // Voegt TTS zin toe aan queue
void startTTS(const String& text);    // Direct TTS (legacy, wordt intern aangeroepen)
uint8_t* getScratchpad();             // Scratchpad voor runtime MP3 arrays
void speakNext();                     // Start volgende item uit queue (called by AudioManager)
void update();
void stop();
}
