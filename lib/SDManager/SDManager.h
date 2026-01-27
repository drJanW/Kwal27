/**
 * @file SDManager.h
 * @brief SD card management interface with directory scanning and file indexing
 * @version 251231E
 * @date 2025-12-31
 *
 * Defines the SDManager singleton class for SD card operations.
 * Key features:
 * - SD card initialization (begin) with SPI configuration
 * - Ready/busy state management with atomic flags
 * - Directory entry (DirEntry) and file entry (FileEntry) structures for indexing
 * - Directory scanning with callback support (SDListCallback)
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
    uint8_t  score;     // 1..200, 0=leeg
    uint8_t  reserved;
};

typedef void (*SDListCallback)(const char* name, bool isDirectory, uint32_t sizeBytes, void* context);

class SDManager {
public:
    static SDManager& instance();

    static bool isReady();
    static void setReady(bool ready);
    static bool isSDbusy();
    static void setSDbusy(bool busy);

    SDManager(const SDManager&) = delete;
    SDManager& operator=(const SDManager&) = delete;

    bool begin(uint8_t csPin);
    bool begin(uint8_t csPin, SPIClass& spi, uint32_t hz);

    void rebuildIndex();
    void scanDirectory(uint8_t dir_num);

    void setHighestDirNum();
    uint8_t getHighestDirNum() const;

    bool readDirEntry (uint8_t dir_num, DirEntry* entry);
    bool writeDirEntry(uint8_t dir_num, const DirEntry* entry);
    bool readFileEntry(uint8_t dir_num, uint8_t file_num, FileEntry* entry);
    bool writeFileEntry(uint8_t dir_num, uint8_t file_num, const FileEntry* entry);

    bool   fileExists(const char* fullPath);
    bool   writeTextFile(const char* path, const char* text);
    String readTextFile(const char* path);
    bool   deleteFile(const char* path);

    // File handle access for streaming (caller must close file)
    // Returns invalid File() if SD already busy
    // Sets SDbusy=true on success, caller must call closeFile() when done
    File   openFileRead(const char* path);
    File   openFileWrite(const char* path);
    void   closeFile(File& file);  // Sets SDbusy=false

    void rebuildWordsIndex();

private:
    SDManager() : highestDirNum(0), ready_(false), sdBusy_(false) {}

private:
    uint8_t highestDirNum;
    std::atomic<bool> ready_;
    std::atomic<bool> sdBusy_;
};

// Pad generator: "/DDD/FFF.mp3"
const char* getMP3Path(uint8_t dirID, uint8_t fileID);
