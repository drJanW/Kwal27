/**
 * @file SDController.h
 * @brief SD card control interface with directory scanning and file indexing
 * @version 260202A
 $12026-02-05
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
 * @brief Pure static SD card controller
 * 
 * All methods are static - no instance needed.
 * Usage: SDController::begin(...), SDController::readTextFile(...), etc.
 */
class SDController {
public:
    // Prevent instantiation
    SDController() = delete;
    SDController(const SDController&) = delete;
    SDController& operator=(const SDController&) = delete;

    // === Initialization ===
    static bool begin(uint8_t csPin);
    static bool begin(uint8_t csPin, SPIClass& spi, uint32_t hz);

    // === State management ===
    static void setReady(bool ready);
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