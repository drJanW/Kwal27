/**
 * @file SDSettings.h
 * @brief Centralized SD card configuration constants and index format definitions
 * @version 251231E
 * @date 2025-12-31
 *
 * Defines compile-time constants for SD card media index system.
 * Configuration includes:
 * - SD_MAX_DIRS: Maximum number of directories (200)
 * - SD_MAX_FILES_PER_SUBDIR: Maximum files per directory (101)
 * - BYTES_PER_MS: Bitrate conversion factor (16 bytes/ms at 128kbps)
 * - Index file paths (.root_dirs, .files_dir, .words_dir)
 * - SD_INDEX_VERSION: Human-readable index format specification
 * - SDPATHLENGTH: Maximum path string length (32 chars)
 */

#pragma once

// Centralised SD card configuration for the media index.
#define SD_MAX_DIRS 200
#define SD_MAX_FILES_PER_SUBDIR 101
#define BYTES_PER_MS 16
#define HEADER_MS 160
#define WORDS_SUBDIR_ID 0
#define ROOT_DIRS "/.root_dirs"
#define FILES_DIR "/.files_dir"
#define WORDS_INDEX_FILE "/000/.words_dir"
#define SD_VERSION_FILENAME "/version.txt"
#define SD_VERSION "V2.01"
#define SDPATHLENGTH 32

#define SD_STRINGIFY_IMPL(x) #x
#define SD_STRINGIFY(x) SD_STRINGIFY_IMPL(x)

#define SD_INDEX_VERSION  \
"SDCONTROLLER " SD_VERSION "\n" \
"Index format V3\n" \
"All MP3 files: mono, 128 kbps (max)\n" \
"headless files only (no ID3 tags)\n" \
"Bitrate: 128 kbps = 16,000 bytes/sec (1 ms ≈ " SD_STRINGIFY(BYTES_PER_MS) " bytes)\n" \
"Estimate ms = (file size / " SD_STRINGIFY(BYTES_PER_MS) ") - " SD_STRINGIFY(HEADER_MS) " [header]\n" \
"Folders: /NNN/   (NNN=0.." SD_STRINGIFY(SD_MAX_DIRS) ", max " SD_STRINGIFY(SD_MAX_DIRS) ")\n" \
"Files:   MMM.mp3 (MMM=1.." SD_STRINGIFY(SD_MAX_FILES_PER_SUBDIR) ", max " SD_STRINGIFY(SD_MAX_FILES_PER_SUBDIR) " per folder)\n" \
"Special: /000/ = reserved/skip\n"
