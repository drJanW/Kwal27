/**
 * @file AudioDirector.cpp
 * @brief Audio fragment selection logic implementation
 * @version 260219E
 * @date 2026-02-19
 */
#include "AudioDirector.h"

#ifndef LOG_AUDIO_DIRECTOR_VERBOSE
#define LOG_AUDIO_DIRECTOR_VERBOSE 0
#endif

#include "Globals.h"
#include "SDController.h"
#include "AudioPolicy.h"
#include "AudioShiftTable.h"
#include "StatusFlags.h"
#include "TodayState.h"

namespace {

using DirScore   = uint32_t;
using FileScore  = uint32_t;

struct DirPick {
    uint8_t  id{};
    DirEntry entry{};
};

bool isAllowed(uint8_t dir, const uint8_t* allowList, size_t allowCount) {
    if (!allowList || allowCount == 0) {
        return true;
    }
    for (size_t i = 0; i < allowCount; ++i) {
        if (allowList[i] == dir) {
            return true;
        }
    }
    return false;
}

bool selectDirectory(DirPick& outDir, const uint8_t* allowList = nullptr, size_t allowCount = 0) {
    DirPick scored[SD_MAX_DIRS];
    uint8_t scoredCount = 0;
    DirScore totalScore = 0;

    auto recordEntry = [&](uint8_t dirNum, const DirEntry& entry) {
        if (entry.fileCount == 0 || entry.totalScore == 0) {
            return;
        }
        if (scoredCount < SD_MAX_DIRS) {
            scored[scoredCount].id = dirNum;
            scored[scoredCount].entry = entry;
            totalScore += static_cast<DirScore>(entry.totalScore);
            ++scoredCount;
        }
    };

    if (allowList && allowCount > 0) {
        for (size_t idx = 0; idx < allowCount; ++idx) {
            const uint8_t dirNum = allowList[idx];
            DirEntry entry{};
            if (!SDController::readDirEntry(dirNum, &entry)) {
                continue;
            }
            recordEntry(dirNum, entry);
        }
    } else {
        File rootFile = SDController::openFileRead(ROOT_DIRS);
        if (!rootFile) {
            PF("[AudioDirector] Can't open %s\n", ROOT_DIRS);
            return false;
        }

        const uint8_t highestDir = SDController::getHighestDirNum();
        for (uint16_t dirNum = 1; dirNum <= highestDir; ++dirNum) {
            if (!isAllowed(static_cast<uint8_t>(dirNum), allowList, allowCount)) {
                continue;
            }

            const uint32_t offset = (uint32_t)(dirNum - 1U) * (uint32_t)sizeof(DirEntry);
            if (!rootFile.seek(offset)) {
                continue;
            }

            DirEntry entry{};
            if (rootFile.read(reinterpret_cast<uint8_t*>(&entry), sizeof(DirEntry)) != sizeof(DirEntry)) {
                continue;
            }
            recordEntry(static_cast<uint8_t>(dirNum), entry);
        }
        SDController::closeFile(rootFile);
    }

    if (totalScore == 0 || scoredCount == 0) {
        if (allowList && allowCount > 0) {
            PF("[AudioDirector] No weighted directories for active theme filter\n");
        } else {
            PF("[AudioDirector] No weighted directories available\n");
        }
        return false;
    }

#if LOG_AUDIO_DIRECTOR_VERBOSE
    PF("[AudioDirector] dir pool count=%u totalScore=%lu\n",
       static_cast<unsigned>(scoredCount),
       static_cast<unsigned long>(totalScore));
#endif

    DirScore ticket = static_cast<DirScore>(random((long)totalScore)) + 1U;
#if LOG_AUDIO_DIRECTOR_VERBOSE
    PF("[AudioDirector] dir ticket=%lu\n", static_cast<unsigned long>(ticket));
#endif
    DirScore cumulative = 0;
    for (uint8_t i = 0; i < scoredCount; ++i) {
        cumulative += static_cast<DirScore>(scored[i].entry.totalScore);
        if (ticket <= cumulative) {
#if LOG_AUDIO_DIRECTOR_VERBOSE
            PF("[AudioDirector] dir pick=%03u score=%u cumulative=%lu\n",
               scored[i].id,
               static_cast<unsigned>(scored[i].entry.totalScore),
               static_cast<unsigned long>(cumulative));
#endif
            outDir = scored[i];
            return true;
        }
    }

    // Should not reach here, but guard anyway.
    outDir = scored[scoredCount - 1];
    return true;
}

bool selectFile(const DirPick& dirPick, uint8_t& outFile) {
    char filesDirPath[SDPATHLENGTH];
    snprintf(filesDirPath, sizeof(filesDirPath), "/%03u%s", dirPick.id, FILES_DIR);

    File filesIndex = SDController::openFileRead(filesDirPath);
    if (!filesIndex) {
        PF("[AudioDirector] Can't open %s\n", filesDirPath);
        return false;
    }

    FileScore totalScore = 0;
    uint16_t candidateCount = 0;
    for (uint16_t fileNum = 1; fileNum <= SD_MAX_FILES_PER_SUBDIR; ++fileNum) {
        FileEntry entry{};
        if (filesIndex.read(reinterpret_cast<uint8_t*>(&entry), sizeof(FileEntry)) != sizeof(FileEntry)) {
            break;
        }
        if (entry.sizeKb == 0 || entry.score == 0) {
            continue;
        }
        ++candidateCount;
        totalScore += static_cast<FileScore>(entry.score);
    }

    if (totalScore == 0) {
        SDController::closeFile(filesIndex);
        PF("[AudioDirector] No weighted files in dir %03u\n", dirPick.id);
        return false;
    }

#if LOG_AUDIO_DIRECTOR_VERBOSE
    PF("[AudioDirector] file pool dir=%03u count=%u totalScore=%lu\n",
       dirPick.id,
       static_cast<unsigned>(candidateCount),
       static_cast<unsigned long>(totalScore));
#endif

    const FileScore target = static_cast<FileScore>(random((long)totalScore)) + 1U;
#if LOG_AUDIO_DIRECTOR_VERBOSE
    PF("[AudioDirector] file ticket=%lu\n", static_cast<unsigned long>(target));
#endif
    filesIndex.seek(0);
    FileScore cumulative = 0;
    for (uint16_t fileNum = 1; fileNum <= SD_MAX_FILES_PER_SUBDIR; ++fileNum) {
        FileEntry entry{};
        if (filesIndex.read(reinterpret_cast<uint8_t*>(&entry), sizeof(FileEntry)) != sizeof(FileEntry)) {
            break;
        }
        if (entry.sizeKb == 0 || entry.score == 0) {
            continue;
        }
        cumulative += static_cast<FileScore>(entry.score);
        if (target <= cumulative) {
            SDController::closeFile(filesIndex);
#if LOG_AUDIO_DIRECTOR_VERBOSE
            PF("[AudioDirector] file pick=%03u score=%u cumulative=%lu\n",
               static_cast<unsigned>(fileNum),
               static_cast<unsigned>(entry.score),
               static_cast<unsigned long>(cumulative));
#endif
            outFile = static_cast<uint8_t>(fileNum);
            return true;
        }
    }

    SDController::closeFile(filesIndex);
    PF("[AudioDirector] Weighted walk failed in dir %03u\n", dirPick.id);
    return false;
}

} // namespace

void AudioDirector::plan() {
    PL_BOOT("[Run][Plan] TODO: move audio orchestration here");
}

bool AudioDirector::selectRandomFragment(AudioFragment& outFrag) {
    // Reset to base theme box before merging (removes previous merge additions)
    AudioPolicy::resetToBaseThemeBox();
    
    // Merge additional theme boxes from audio shifts
    uint64_t statusBits = StatusFlags::getFullStatusBits();
    auto additions = AudioShiftTable::instance().getThemeBoxAdditions(statusBits);
    if (!additions.empty()) {
        for (uint8_t boxId : additions) {
            const ThemeBox* box = FindThemeBox(boxId);
            if (box && !box->entries.empty()) {
                // Convert uint16_t entries to uint8_t array
                std::vector<uint8_t> dirs;
                dirs.reserve(box->entries.size());
                for (uint16_t e : box->entries) {
                    if (e <= 255) {
                        dirs.push_back(static_cast<uint8_t>(e));
                    }
                }
                if (!dirs.empty()) {
                    AudioPolicy::mergeThemeBoxDirs(dirs.data(), dirs.size());
                }
            }
        }
    }

    // Get fadeMs early - needed for minDuration calculation
    // Randomize ±50% around context-computed value for per-fragment variation
    const uint16_t contextFade = AudioShiftTable::instance().getFadeMs(statusBits);
    const uint16_t fadeLo = static_cast<uint16_t>(max(500, contextFade / 2));
    const uint16_t fadeHi = static_cast<uint16_t>(min(60000, contextFade * 3 / 2));
    const uint16_t fadeMs = static_cast<uint16_t>(random(fadeLo, fadeHi + 1));
    const uint32_t minDuration = 2U * fadeMs + 100U;

    // Retry loop: try up to 15 different files if playback window cannot be satisfied
    for (int fileAttempt = 0; fileAttempt < 15; ++fileAttempt) {
        DirPick dirPick{};

        size_t themeCount = 0;
        const uint8_t* themeDirs = AudioPolicy::themeBoxDirs(themeCount);
        if (themeDirs && themeCount > 0) {
            if (!selectDirectory(dirPick, themeDirs, themeCount)) {
                PF("[AudioDirector] Theme box %s unavailable, falling back to full directory pool\n",
                   AudioPolicy::themeBoxId().c_str());
                if (!selectDirectory(dirPick)) {
                    return false;
                }
            }
        } else {
            if (!selectDirectory(dirPick)) {
                return false;
            }
        }

        uint8_t file = 0;
        if (!selectFile(dirPick, file)) {
            continue;  // Try another directory/file
        }

        FileEntry fileEntry{};
        if (!SDController::readFileEntry(dirPick.id, file, &fileEntry)) {
            PF("[AudioDirector] Failed to read file entry %03u/%03u\n", dirPick.id, file);
            continue;
        }

        const uint32_t rawDuration = static_cast<uint32_t>(fileEntry.sizeKb) * 1024UL / BYTES_PER_MS;
        if (rawDuration <= HEADER_MS + minDuration) {
            PF("[AudioDirector] Fragment candidate too short %03u/%03u (raw=%lu min=%lu)\n", 
               dirPick.id, file, static_cast<unsigned long>(rawDuration), static_cast<unsigned long>(minDuration));
            continue;
        }

        // Calculate start window: random within first fragmentStartFraction% of track
        const uint32_t maxStartMs = (rawDuration * Globals::fragmentStartFraction) / 100U;
        const uint32_t startLow = HEADER_MS;
        const uint32_t startHigh = (maxStartMs > startLow) ? maxStartMs : startLow;

        // Try up to 15 start positions within this file
        for (int startAttempt = 0; startAttempt < 15; ++startAttempt) {
            const uint32_t startMs = (startLow < startHigh) 
                ? static_cast<uint32_t>(random(startLow, startHigh + 1))
                : startLow;
            
            const uint32_t maxDuration = rawDuration - startMs;
            
            if (maxDuration >= minDuration) {
                // Valid window found - randomize duration
                const uint32_t durationMs = static_cast<uint32_t>(random(minDuration, maxDuration + 1));
                
                outFrag.dirIndex   = dirPick.id;
                outFrag.fileIndex  = file;
                outFrag.score      = fileEntry.score;
                outFrag.startMs    = static_cast<uint16_t>(startMs);
                outFrag.durationMs = static_cast<uint16_t>((durationMs > 0xFFFF) ? 0xFFFF : durationMs);
                outFrag.fadeMs     = fadeMs;
                // source filled by caller (requestPlayFragment sets timer/next)

#if LOG_AUDIO_DIRECTOR_VERBOSE
                PF("[AudioDirector] pick %03u/%03u start=%u dur=%u fade=%u (raw=%lu)\n",
                   outFrag.dirIndex, outFrag.fileIndex,
                   static_cast<unsigned>(outFrag.startMs),
                   static_cast<unsigned>(outFrag.durationMs),
                   static_cast<unsigned>(outFrag.fadeMs),
                   static_cast<unsigned long>(rawDuration));
#endif

                return true;
            }
        }
        
        PF("[AudioDirector] Could not find valid window for %03u/%03u, trying another file\n", dirPick.id, file);
    }

    PF("[AudioDirector] Failed to find valid fragment after 5 file attempts\n");
    return false;
}
