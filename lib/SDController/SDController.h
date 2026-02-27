/**
 * @file SDController.h
 * @brief SD card control interface with directory scanning and file indexing
 * @version 260227B
 * @date 2026-02-27
 */
#pragma once
#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <atomic>
#include "Globals.h"
#include "SDSettings.h"
#include "SdFileAccess.h"

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
    static bool checkPresent();  // Probe card presence (cardType check)
    static void lockSD()  { SdFileAccess::lock(); }    // delegates to SdFileAccess
    static void unlockSD() { SdFileAccess::unlock(); }  // delegates to SdFileAccess

    // === Index operations ===
    static void rebuildIndex();
    static void scanDirectory(uint8_t dir_num);
    static void syncDirectory(uint8_t dir_num);  // Like scanDirectory but preserves existing votes
    static void rebuildWordsIndex();
    static void updateHighestDirNum();
    static uint8_t getHighestDirNum();

    // === Entry read/write ===
    static bool readDirEntry (uint8_t dir_num, DirEntry* entry);
    static bool writeDirEntry(uint8_t dir_num, const DirEntry* entry);
    static bool readFileEntry(uint8_t dir_num, uint8_t file_num, FileEntry* entry);
    static bool writeFileEntry(uint8_t dir_num, uint8_t file_num, const FileEntry* entry);

    // === File operations (delegate to SdFileAccess) ===
    static bool   fileExists(const char* fullPath) { return SdFileAccess::fileExists(fullPath); }
    static bool   writeTextFile(const char* path, const char* text) { return SdFileAccess::writeTextFile(path, text); }
    static String readTextFile(const char* path) { return SdFileAccess::readTextFile(path); }
    static bool   deleteFile(const char* path) { return SdFileAccess::deleteFile(path); }

    // === Streaming file access (delegate to SdFileAccess) ===
    static File openFileRead(const char* path) { return SdFileAccess::openRead(path); }
    static File openFileWrite(const char* path) { return SdFileAccess::openWrite(path); }
    static void closeFile(File& file) { SdFileAccess::closeFile(file); }

private:
    static std::atomic<bool> ready_;
    static uint8_t highestDirNum_;
};