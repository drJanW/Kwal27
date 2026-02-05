/**
 * @file WebGuiStatus.h
 * @brief Centralized WebGUI state management
 * @version 260202A
 * @date 2026-02-02
 */
#pragma once

#include <Arduino.h>
#include <atomic>

// Forward declaration
class AsyncEventSource;

namespace WebGuiStatus {

// ============================================================================
// Setters (firmware → struct → SSE push)
// ============================================================================

/**
 * @brief Set brightness and push state to browser
 * @param value Brightness 0-255
 */
void setBrightness(uint8_t value);

/**
 * @brief Set audio level and push state to browser
 * @param value Audio level 0.0-1.0
 */
void setAudioLevel(float value);

/**
 * @brief Set current fragment info and push state to browser
 * @param dir Directory number (0 = no fragment)
 * @param file File number (0 = no fragment)
 * @param score Current vote score
 * @param durationMs Fragment duration for UI timeout (0 = use default)
 */
void setFragment(uint8_t dir, uint8_t file, uint8_t score, uint32_t durationMs = 0);

/**
 * @brief Update only the fragment score (for vote updates)
 * @param score New vote score (0-200)
 */
void setFragmentScore(uint8_t score);

// ============================================================================
// Getters (fragment state only - brightness/audio read from source modules)
// ============================================================================

uint8_t getFragmentDir();
uint8_t getFragmentFile();
uint8_t getFragmentScore();

// ============================================================================
// SSE Push functions
// ============================================================================

/**
 * @brief Push current state to all connected browsers
 * Builds JSON with brightness, audioLevel, patternId, colorId, fragment
 */
void pushState();

/**
 * @brief Push patterns list to all connected browsers
 * Called after pattern CRUD operations
 */
void pushPatterns();

/**
 * @brief Push colors list to all connected browsers
 * Called after color CRUD operations
 */
void pushColors();

/**
 * @brief Push all three events (for reconnect)
 * Order: patterns → colors → state
 */
void pushAll();

// ============================================================================
// Initialization
// ============================================================================

/**
 * @brief Initialize WebGuiStatus with defaults
 * Called during beginWebInterface()
 */
void begin();

/**
 * @brief Set SSE event source pointer
 * Called from SseController::setup() to enable SSE push
 * @param events Pointer to AsyncEventSource
 */
void setEventSource(AsyncEventSource* events);

} // namespace WebGuiStatus
