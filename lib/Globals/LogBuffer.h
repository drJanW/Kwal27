#pragma once
/**
 * @file LogBuffer.h
 * @brief LogBuffer implementation
 * @version 260202A
 * @date 2026-02-02
 */
#include <cstddef>

namespace LogBuffer {

// WARNING: Static .bss allocation — eats into heap available for MP3 decoder.
// 32KB caused audio decode failures (no PSRAM). 16KB marginal with NasBackup TCP.
constexpr size_t BUFFER_SIZE = 12288;

// Timestamp provider function type: fills buffer with "HH:MM:SS " (9 chars + null)
// Returns true if timestamp was written, false to skip timestamp
using TimestampProvider = bool (*)(char* buf, size_t bufSize);

// Set timestamp provider (called by ClockController after init)
void setTimestampProvider(TimestampProvider provider);

// Get current timestamp string (returns empty if no provider or provider returns false)
// Writes "HH:MM:SS " format to buffer, returns chars written (0 if none)
size_t getTimestamp(char* buf, size_t bufSize);

void appendLine(const char* msg);
void appendf(const char* fmt, ...);
size_t read(char* out, size_t maxLen);
void clear();
size_t available();

} // namespace LogBuffer
