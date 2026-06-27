# SdFileAccess — SD Card I/O Abstraction Layer

**Role in Kwal27:** Provides thread-safe, reentrant-locked access to the SD card filesystem. All SD reads/writes go through `SdFileAccess` to prevent concurrent access from timer callbacks (audio playback) and web handlers (file uploads, votes). Path utilities, config constants, and a RAII guard complete the layer.

## Files

### SdFileAccess.h / SdFileAccess.cpp
Reentrant-lock-based SD filesystem gateway. `lock()` increments a counter, `unlock()` decrements it — only the outermost unlock actually releases the SPI bus. This allows nested usage: audio reading a fragment while an index update holds the lock.

Core API:
- `lock()` / `unlock()` — acquire/release SD access (reentrant counter)
- `readFile(path)` — read entire text file into String
- `readBinaryFile(path)` — read binary file into allocated buffer
- `writeFile(path, data, len)` — write binary data
- `exists(path)` / `remove(path)` / `mkdir(path)` — basic filesystem ops
- `openRead(path)` — open read stream (for MP3 playback streaming)

### SdPathUtils.h / SdPathUtils.cpp
SD path manipulation utilities. API:
- `sanitizeSdPath(raw)` — normalise slashes, strip dangerous chars
- `sanitizeSdFilename(raw)` — sanitize uploaded filenames
- `buildUploadTarget(dir, filename)` — construct full path for file upload
- `parentPath(path)` — get parent directory
- `extractBaseName(fullPath)` — get filename portion
- `removeSdPath(targetPath)` — safe recursive delete

### SDSettings.h
Centralized SD card configuration constants (header-only):
- `SD_MAX_DIRS` = 200 — max audio directories
- `SD_MAX_FILES_PER_SUBDIR` = 101 — max MP3s per directory
- `BYTES_PER_MS` = 16 — 128 kbps MP3 byte rate
- `HEADER_MS` = 160 — estimated ID3-free header overhead
- `WORDS_SUBDIR_ID` = 0 — reserved `/000/` for TTS words
- Index file paths: `ROOT_DIRS`, `FILES_DIR`, `WORDS_INDEX_FILE`
- `SD_VERSION` = `"V2.01"` — index format version string

### SDBusyGuard.h
RAII guard class. Constructor calls `SdFileAccess::lock()`, destructor calls `unlock()`. Since locking is reentrant, `acquired()` always returns true. Use `release()` to manually unlock before the guard goes out of scope. Every function that accesses SD should either use `SDBusyGuard` or document why locking is handled externally.