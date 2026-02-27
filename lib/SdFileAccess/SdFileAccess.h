/**
 * @file SdFileAccess.h
 * @brief Shared SD card lock and basic file I/O
 * @version 260227B
 * @date 2026-02-27
 *
 * Provides SD bus locking and basic file operations available to any layer.
 * Extracted from SDController to eliminate cross-controller coupling.
 * SDController still owns index operations and directory scanning.
 */
#pragma once

#include <Arduino.h>
#include <SD.h>

namespace SdFileAccess {

    /// Increment reentrant lock counter; marks SD busy on first lock
    void lock();

    /// Decrement reentrant lock counter; clears SD busy when last lock released
    void unlock();

    /// Check if file exists at the given absolute SD path
    bool fileExists(const char* path);

    /// Write text to a file (overwrites). Returns true on success.
    bool writeTextFile(const char* path, const char* text);

    /// Read entire text file into a String. Returns "" on failure.
    String readTextFile(const char* path);

    /// Delete a file. Returns true if deleted.
    bool deleteFile(const char* path);

    /// Open file for reading. Caller MUST call closeFile() when done.
    /// Returns invalid File() on failure (lock is NOT held on failure).
    File openRead(const char* path);

    /// Open file for writing. Caller MUST call closeFile() when done.
    /// Returns invalid File() on failure (lock is NOT held on failure).
    File openWrite(const char* path);

    /// Close a file opened via openRead/openWrite and release the SD lock.
    void closeFile(File& file);

} // namespace SdFileAccess

/// Build "/DDD/FFF.mp3" path from directory and file indices
const char* getMP3Path(uint8_t dirID, uint8_t fileID);
