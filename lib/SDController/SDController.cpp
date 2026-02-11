/**
 * @file SDController.cpp
 * @brief SD card control implementation with directory scanning and file indexing
 * @version 260202A
 $12026-02-05
 */
#include <Arduino.h>
#include "SDController.h"
#include "SdPathUtils.h"
#include "Alert/AlertState.h"
#include <cstring>

// NOTE: File timestamps use system time set by PRTClock via settimeofday().
// ESP32's built-in FatFs get_fattime() reads from system time automatically.

namespace {
using SdPathUtils::extractBaseName;
using SdPathUtils::removeSdPath;
} // namespace

// === Static member definitions ===
std::atomic<bool> SDController::ready_{false};
std::atomic<uint8_t> SDController::lockCount_{0};
uint8_t SDController::highestDirNum_{0};

// === Initialization ===

bool SDController::begin(uint8_t csPin) { 
    return SD.begin(csPin); 
}

bool SDController::begin(uint8_t csPin, SPIClass& spi, uint32_t hz) { 
    return SD.begin(csPin, spi, hz); 
}

// === State management ===

void SDController::setReady(bool ready) {
    ready_.store(ready, std::memory_order_relaxed);
    AlertState::setStatusOK(SC_SD, ready);
}

void SDController::lockSD() {
    uint8_t prev = lockCount_.fetch_add(1, std::memory_order_relaxed);
    if (prev == 0) {
        AlertState::setSdBusy(true);
    }
}

void SDController::unlockSD() {
    uint8_t prev = lockCount_.load(std::memory_order_relaxed);
    if (prev > 0) {
        uint8_t before = lockCount_.fetch_sub(1, std::memory_order_relaxed);
        if (before == 1) {
            AlertState::setSdBusy(false);
        }
    }
}

// === Index operations ===

void SDController::rebuildIndex() {
    lockSD();
    if (SD.exists(ROOT_DIRS)) {
        SD.remove(ROOT_DIRS);
    }
    File root = SD.open(ROOT_DIRS, FILE_WRITE);
    if (!root) {
        PF("[SDController] Cannot create %s\n", ROOT_DIRS);
        unlockSD();
        return;
    }
    DirEntry empty = {0, 0};
    for (uint16_t i = 0; i < SD_MAX_DIRS; i++) {
        root.write((const uint8_t*)&empty, sizeof(DirEntry));
    }
    root.close();

    uint16_t preservedDirs = 0;
    uint16_t rebuiltDirs = 0;

    // Dir 000 is words/speak - handled separately, skip here
    for (uint16_t d = 1; d <= SD_MAX_DIRS; ++d) {
        char dirPath[12];
        snprintf(dirPath, sizeof(dirPath), "/%03u", static_cast<unsigned>(d));
        if (!SD.exists(dirPath)) {
            continue;
        }

        char filesDirPath[SDPATHLENGTH];
        snprintf(filesDirPath, sizeof(filesDirPath), "%s%s", dirPath, FILES_DIR);

        if (!SD.exists(filesDirPath)) {
            scanDirectory(static_cast<uint8_t>(d));
            ++rebuiltDirs;
            continue;
        }

        File filesIndex = SD.open(filesDirPath, FILE_READ);
        if (!filesIndex) {
            PF("[SDController] Unable to read %s, rebuilding directory\n", filesDirPath);
            scanDirectory(static_cast<uint8_t>(d));
            ++rebuiltDirs;
            continue;
        }

        const uint32_t expectedSize = static_cast<uint32_t>(SD_MAX_FILES_PER_SUBDIR) * sizeof(FileEntry);
        const uint32_t actualSize = static_cast<uint32_t>(filesIndex.size());
        if (actualSize != expectedSize) {
            PF("[SDController] Corrupt index %s (size=%lu expected=%lu), rebuilding\n",
               filesDirPath,
               static_cast<unsigned long>(actualSize),
               static_cast<unsigned long>(expectedSize));
            filesIndex.close();
            scanDirectory(static_cast<uint8_t>(d));
            ++rebuiltDirs;
            continue;
        }

        DirEntry dirEntry{0, 0};
        for (uint16_t fnum = 1; fnum <= SD_MAX_FILES_PER_SUBDIR; ++fnum) {
            FileEntry fe{};
            if (filesIndex.read(reinterpret_cast<uint8_t*>(&fe), sizeof(FileEntry)) != sizeof(FileEntry)) {
                break;
            }
            if (fe.sizeKb == 0 || fe.score == 0) {
                continue;
            }
            ++dirEntry.fileCount;
            dirEntry.totalScore += fe.score;
        }
        filesIndex.close();

        if (!writeDirEntry(static_cast<uint8_t>(d), &dirEntry)) {
            PF("[SDController] Failed to update dir entry %03u\n", static_cast<unsigned>(d));
        } else if (dirEntry.fileCount > 0) {
            ++preservedDirs;
        }
    }

    rebuildWordsIndex();

    File v = SD.open(SD_VERSION_FILENAME, FILE_WRITE);
    if (v) {
        v.print(SD_INDEX_VERSION);
        v.close();
        PF("[SDController] Wrote version %s\n", SD_INDEX_VERSION);
    }

    updateHighestDirNum();

    PF("[SDController] Index rebuild complete (preserved=%u rebuilt=%u).\n",
       static_cast<unsigned>(preservedDirs),
       static_cast<unsigned>(rebuiltDirs));
    unlockSD();
}

void SDController::scanDirectory(uint8_t dir_num) {
    // Note: caller should have called lockSD()
    char dirPath[12];
    snprintf(dirPath, sizeof(dirPath), "/%03u", dir_num);
    char filesDirPath[SDPATHLENGTH];
    snprintf(filesDirPath, sizeof(filesDirPath), "%s%s", dirPath, FILES_DIR);

    if (SD.exists(filesDirPath)) {
        SD.remove(filesDirPath);
    }
    File filesIndex = SD.open(filesDirPath, "w");
    if (!filesIndex) {
        PF("[SDController] Open fail: %s\n", filesDirPath);
        return;
    }

    DirEntry dirEntry = {0, 0};

    for (uint8_t fnum = 1; fnum <= SD_MAX_FILES_PER_SUBDIR; fnum++) {
        FileEntry fe = {0, 0, 0};
        char mp3path[SDPATHLENGTH];
        snprintf(mp3path, sizeof(mp3path), "%s/%03u.mp3", dirPath, fnum);
        if (SD.exists(dirPath) && SD.exists(mp3path)) {
            File mp3 = SD.open(mp3path, FILE_READ);
            if (mp3) {
                fe.sizeKb = mp3.size() / 1024;
                mp3.close();
            }
            fe.score = 100;
            dirEntry.fileCount++;
            dirEntry.totalScore += fe.score;
        }
        filesIndex.seek((fnum - 1) * sizeof(FileEntry));
        filesIndex.write((const uint8_t*)&fe, sizeof(FileEntry));
    }
    filesIndex.close();

    if (SD.exists(dirPath)) {
        writeDirEntry(dir_num, &dirEntry);
    }
}

void SDController::rebuildWordsIndex() {
    // Note: caller should have called lockSD()
    if (SD.exists(WORDS_INDEX_FILE)) {
        SD.remove(WORDS_INDEX_FILE);
    }
    File idx = SD.open(WORDS_INDEX_FILE, FILE_WRITE);
    if (!idx) {
        PF("[SDController] Failed to create %s\n", WORDS_INDEX_FILE);
        return;
    }

    char mp3Path[SDPATHLENGTH];
    for (uint16_t wordId = 0; wordId < SD_MAX_FILES_PER_SUBDIR; ++wordId) {
        uint16_t durationMs = 0;
        snprintf(mp3Path, sizeof(mp3Path), "/%03u/%03u.mp3", WORDS_SUBDIR_ID, wordId);
        if (SD.exists(mp3Path)) {
            File mp3 = SD.open(mp3Path, FILE_READ);
            if (mp3) {
                uint32_t sizeBytes = mp3.size();
                mp3.close();
                
                // Empirical formula: duration_ms = (size_bytes * 5826) / 100000
                uint32_t audioMs = (sizeBytes * 5826UL) / 100000UL;
                if (audioMs > 0xFFFF) {
                    audioMs = 0xFFFF;
                } else if (audioMs == 0 && sizeBytes > 0) {
                    audioMs = 100;
                }
                durationMs = static_cast<uint16_t>(audioMs);
            }
        }
        idx.write(reinterpret_cast<const uint8_t*>(&durationMs), sizeof(durationMs));
    }
    idx.close();
    PF("[SDController] Rebuilt %s\n", WORDS_INDEX_FILE);
}

void SDController::updateHighestDirNum() {
    // Note: caller should have called lockSD()
    highestDirNum_ = 0;
    uint16_t dirCount = 0;
    uint32_t totalFiles = 0;
    DirEntry e;
    for (int16_t d = SD_MAX_DIRS; d >= 1; --d) {
        if (readDirEntry(d, &e) && e.fileCount > 0) {
            if (highestDirNum_ == 0) {
                highestDirNum_ = d;  // First hit = highest
            }
            ++dirCount;
            totalFiles += e.fileCount;
        }
    }
    PF_BOOT("[SDController] %u dirs, %lu files\n",
       dirCount, static_cast<unsigned long>(totalFiles));
}

uint8_t SDController::getHighestDirNum() {
    return highestDirNum_;
}

// === Entry read/write ===

bool SDController::readDirEntry(uint8_t dir_num, DirEntry* entry) {
    lockSD();
    File f = SD.open(ROOT_DIRS, FILE_READ);
    if (!f) {
        unlockSD();
        return false;
    }
    f.seek((dir_num - 1) * sizeof(DirEntry));
    bool ok = f.read((uint8_t*)entry, sizeof(DirEntry)) == sizeof(DirEntry);
    f.close();
    unlockSD();
    return ok;
}

bool SDController::writeDirEntry(uint8_t dir_num, const DirEntry* entry) {
    lockSD();
    File f = SD.open(ROOT_DIRS, "r+");
    if (!f) {
        unlockSD();
        return false;
    }
    const uint32_t offset = static_cast<uint32_t>(dir_num - 1U) * sizeof(DirEntry);
    if (!f.seek(offset)) {
        f.close();
        unlockSD();
        return false;
    }
    bool ok = f.write(reinterpret_cast<const uint8_t*>(entry), sizeof(DirEntry)) == sizeof(DirEntry);
    f.close();
    unlockSD();
    return ok;
}

bool SDController::readFileEntry(uint8_t dir_num, uint8_t file_num, FileEntry* entry) {
    lockSD();
    char p[SDPATHLENGTH];
    snprintf(p, sizeof(p), "/%03u%s", dir_num, FILES_DIR);
    File f = SD.open(p, FILE_READ);
    if (!f) {
        unlockSD();
        return false;
    }
    f.seek((file_num - 1) * sizeof(FileEntry));
    bool ok = f.read((uint8_t*)entry, sizeof(FileEntry)) == sizeof(FileEntry);
    f.close();
    unlockSD();
    return ok;
}

bool SDController::writeFileEntry(uint8_t dir_num, uint8_t file_num, const FileEntry* entry) {
    lockSD();
    char p[SDPATHLENGTH];
    snprintf(p, sizeof(p), "/%03u%s", dir_num, FILES_DIR);
    File f = SD.open(p, "r+");
    if (!f) {
        unlockSD();
        return false;
    }
    const uint32_t offset = static_cast<uint32_t>(file_num - 1U) * sizeof(FileEntry);
    if (!f.seek(offset)) {
        f.close();
        unlockSD();
        return false;
    }
    bool ok = f.write(reinterpret_cast<const uint8_t*>(entry), sizeof(FileEntry)) == sizeof(FileEntry);
    f.close();
    unlockSD();
    return ok;
}

// === File operations ===

bool SDController::fileExists(const char* fullPath) {
    lockSD();
    bool exists = SD.exists(fullPath);
    unlockSD();
    return exists;
}

bool SDController::writeTextFile(const char* path, const char* text) {
    lockSD();
    File f = SD.open(path, FILE_WRITE);
    if (!f) {
        unlockSD();
        return false;
    }
    f.print(text);
    f.close();
    unlockSD();
    return true;
}

String SDController::readTextFile(const char* path) {
    lockSD();
    File f = SD.open(path, FILE_READ);
    if (!f) {
        unlockSD();
        return "";
    }
    String s = f.readString();
    f.close();
    unlockSD();
    return s;
}

bool SDController::deleteFile(const char* path) {
    lockSD();
    bool result = SD.exists(path) && SD.remove(path);
    unlockSD();
    return result;
}

// === Streaming file access ===

File SDController::openFileRead(const char* path) {
    if (!path) {
        return File();
    }
    lockSD();
    File f = SD.open(path, FILE_READ);
    if (!f) {
        unlockSD();
    }
    // Note: caller must call closeFile() to unlockSD()
    return f;
}

File SDController::openFileWrite(const char* path) {
    if (!path) {
        return File();
    }
    lockSD();
    File f = SD.open(path, FILE_WRITE);
    if (!f) {
        unlockSD();
    }
    // Note: caller must call closeFile() to unlockSD()
    return f;
}

void SDController::closeFile(File& file) {
    if (file) {
        file.close();
    }
    unlockSD();
}

// === Free function ===

const char* getMP3Path(uint8_t dirID, uint8_t fileID) {
    static char path[SDPATHLENGTH];
    snprintf(path, sizeof(path), "/%03u/%03u.mp3", dirID, fileID);
    return path;
}