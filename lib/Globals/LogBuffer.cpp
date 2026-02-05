/**
 * @file LogBuffer.cpp
 * @brief LogBuffer implementation
 * @version 260127A
 * @date 2026-01-27
 */
#include <Arduino.h>
#include "LogBuffer.h"

namespace LogBuffer {

static char buffer[BUFFER_SIZE];
static size_t head = 0;  // write position
static size_t used = 0;  // bytes in buffer
static TimestampProvider timestampProvider = nullptr;

void setTimestampProvider(TimestampProvider provider) {
    timestampProvider = provider;
}

size_t getTimestamp(char* buf, size_t bufSize) {
    if (!buf || bufSize < 10 || !timestampProvider) return 0;
    if (timestampProvider(buf, bufSize)) {
        return strlen(buf);
    }
    return 0;
}

static void appendChar(char c) {
    buffer[head] = c;
    head = (head + 1) % BUFFER_SIZE;
    if (used < BUFFER_SIZE) used++;
}

void clear() {
    head = 0;
    used = 0;
}

void appendLine(const char* msg) {
    if (!msg) return;
    while (*msg) {
        appendChar(*msg++);
    }
    appendChar('\n');
}

void appendf(const char* fmt, ...) {
    if (!fmt) return;
    char temp[256];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(temp, sizeof(temp), fmt, args);
    va_end(args);
    
    if (len > 0) {
        size_t writeLen = (len < (int)sizeof(temp)) ? len : sizeof(temp) - 1;
        for (size_t i = 0; i < writeLen; i++) {
            appendChar(temp[i]);
        }
    }
}

size_t available() {
    return used;
}

size_t read(char* out, size_t maxLen) {
    if (!out || maxLen == 0 || used == 0) return 0;
    
    size_t toRead = (maxLen < used) ? maxLen : used;
    size_t start = (head + BUFFER_SIZE - used) % BUFFER_SIZE;
    
    for (size_t i = 0; i < toRead; i++) {
        out[i] = buffer[(start + i) % BUFFER_SIZE];
    }
    
    return toRead;
}

} // namespace LogBuffer
