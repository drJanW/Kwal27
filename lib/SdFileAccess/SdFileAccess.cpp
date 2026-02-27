/**
 * @file SdFileAccess.cpp
 * @brief Shared SD card lock and basic file I/O implementation
 * @version 260227B
 * @date 2026-02-27
 *
 * Owns the reentrant SD lock counter and basic file operations.
 * Extracted from SDController to allow cross-layer access without
 * architecture violations.
 */
#include <Arduino.h>
#include "SdFileAccess.h"
#include "Alert/AlertState.h"
#include "SDSettings.h"

#include <atomic>

namespace {
std::atomic<uint8_t> lockCount_{0};
} // namespace

// === Lock / Unlock ===

void SdFileAccess::lock() {
    uint8_t prev = lockCount_.fetch_add(1, std::memory_order_relaxed);
    if (prev == 0) {
        AlertState::setSdBusy(true);
    }
}

void SdFileAccess::unlock() {
    uint8_t prev = lockCount_.load(std::memory_order_relaxed);
    if (prev > 0) {
        uint8_t before = lockCount_.fetch_sub(1, std::memory_order_relaxed);
        if (before == 1) {
            AlertState::setSdBusy(false);
        }
    }
}

// === File operations ===

bool SdFileAccess::fileExists(const char* path) {
    lock();
    bool exists = SD.exists(path);
    unlock();
    return exists;
}

bool SdFileAccess::writeTextFile(const char* path, const char* text) {
    lock();
    File f = SD.open(path, FILE_WRITE);
    if (!f) {
        unlock();
        return false;
    }
    f.print(text);
    f.close();
    unlock();
    return true;
}

String SdFileAccess::readTextFile(const char* path) {
    lock();
    File f = SD.open(path, FILE_READ);
    if (!f) {
        unlock();
        return "";
    }
    String s = f.readString();
    f.close();
    unlock();
    return s;
}

bool SdFileAccess::deleteFile(const char* path) {
    lock();
    bool result = SD.exists(path) && SD.remove(path);
    unlock();
    return result;
}

// === Streaming file access ===

File SdFileAccess::openRead(const char* path) {
    if (!path) {
        return File();
    }
    lock();
    File f = SD.open(path, FILE_READ);
    if (!f) {
        unlock();
    }
    // Note: caller must call closeFile() to unlock()
    return f;
}

File SdFileAccess::openWrite(const char* path) {
    if (!path) {
        return File();
    }
    lock();
    File f = SD.open(path, FILE_WRITE);
    if (!f) {
        unlock();
    }
    // Note: caller must call closeFile() to unlock()
    return f;
}

void SdFileAccess::closeFile(File& file) {
    if (file) {
        file.close();
    }
    unlock();
}

// === Free function ===

const char* getMP3Path(uint8_t dirID, uint8_t fileID) {
    static char path[SDPATHLENGTH];
    snprintf(path, sizeof(path), "/%03u/%03u.mp3", dirID, fileID);
    return path;
}
