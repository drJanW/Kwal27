/**
 * @file SDManager.h
 * @brief SD card management interface with directory scanning and file indexing
 * @version 260128A
 * @date 2026-01-28
 *
 * Pure static SD card manager - no singleton, no instance() overhead.
 * Key features:
 * - SD card initialization (begin) with SPI configuration
 * - Ready/busy state management with atomic flags
 * - Reentrant lockSD()/unlockSD() for nested SD operations
 * - Directory entry (DirEntry) and file entry (FileEntry) structures for indexing
 * - Index building and file path resolution
 * - Support for up to SD_MAX_DIRS directories with SD_MAX_FILES_PER_SUBDIR files each
 */

#pragma once
#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <atomic>
#include "Globals.h"
#include "SDSettings.h"

// ===== index structs =====
struct DirEntry {
    uint16_t fileCount;
    uint16_t totalScore;
};

struct FileEntry {
    uint16_t sizeKb;
    uint8_t  score;     // 1..200, 0=empty
    uint8_t  reserved;
};

typedef void (*SDListCallback)(const char* name, bool isDirectory, uint32_t sizeBytes, void* context);

/**
 * @brief Pure static SD card manager
 * 
 * All methods are static - no instance needed.
 * Usage: SDManager::begin(...), SDManager::readTextFile(...), etc.
 */
class SDManager {
public:
    // Prevent instantiation
    SDManager() = delete;
    SDManager(const SDManager&) = delete;
    SDManager& operator=(const SDManager&) = delete;

    // === Initialization ===
    static bool begin(uint8_t csPin);
    static bool begin(uint8_t csPin, SPIClass& spi, uint32_t hz);

    // === State management ===
    static bool isReady();
    static void setReady(bool ready);
    static bool isSDbusy();      // Returns true if lockCount > 0
    static void lockSD();        // Increment lock counter (reentrant)
    static void unlockSD();      // Decrement lock counter

    // === Index operations ===
    static void rebuildIndex();
    static void scanDirectory(uint8_t dir_num);
    static void rebuildWordsIndex();
    static void updateHighestDirNum();
    static uint8_t getHighestDirNum();

    // === Entry read/write ===
    static bool readDirEntry (uint8_t dir_num, DirEntry* entry);
    static bool writeDirEntry(uint8_t dir_num, const DirEntry* entry);
    static bool readFileEntry(uint8_t dir_num, uint8_t file_num, FileEntry* entry);
    static bool writeFileEntry(uint8_t dir_num, uint8_t file_num, const FileEntry* entry);

    // === File operations ===
    static bool   fileExists(const char* fullPath);
    static bool   writeTextFile(const char* path, const char* text);
    static String readTextFile(const char* path);
    static bool   deleteFile(const char* path);

    // === Streaming file access ===
    // Returns invalid File() on failure
    // On success: lockSD() is called, caller MUST call closeFile() when done
    static File openFileRead(const char* path);
    static File openFileWrite(const char* path);
    static void closeFile(File& file);  // Calls unlockSD()

private:
    static std::atomic<bool> ready_;
    static std::atomic<uint8_t> lockCount_;
    static uint8_t highestDirNum_;
};

// Path generator: "/DDD/FFF.mp3"
const char* getMP3Path(uint8_t dirID, uint8_t fileID);
