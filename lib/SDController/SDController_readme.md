# SDController — SD Card Index & File Manager

**Role in Kwal27:** Scans the SD card filesystem, builds an in-memory index of all MP3 files (duration, vote scores, ban status), and provides fast random file selection by directory. The file index persists as structured binary files on SD for fast warm boots.

## Files

### SDController.h / SDController.cpp
Core SD card filesystem manager. Scans `"/"` for numbered subdirectories (`/001/` through `/200/`), counts MP3 files in each, reads file sizes to estimate playback duration (128 kbps × 16 bytes/ms, minus 160 ms header), and builds cached index files (`.root_dirs`, `.files_dir`, `.words_dir`) for fast warm boot. API:
- `begin()` — mount SD, scan or load cached index
- `getTotalDirs()` / `getFileCount(dir)` — indexed stats
- `getFileSize(dir, file)` — size for duration estimation
- `getTotalVote(dir, file)` — current vote score
- `isBanned(dir, file)` — ban check
- `getDirNumberFromPath()` — path → index mapping
- `debugPrintCatalog()` — dump index to Serial

Global instance: `extern SDController sdController`.

### SDVoting.h / SDVoting.cpp
Audio fragment voting and ban system. Provides:
- `getRandomFile(dir)` — weighted random selection using vote scores (higher score = more likely)
- `applyVote(dir, file, delta)` — increment/decrement vote score (web UI driven)
- `saveAccumulatedVotes()` — persist vote deltas to SD card
- `banFile(dir, file)` — permanently hide a file from selection
- `deleteIndexedFile(dir, file)` — remove file from SD
- `getCurrentPlayable()` — returns dir/file of currently playing fragment for web UI display
- `attachVoteRoute(server)` — register vote REST endpoints on the web server

Votes accumulate in RAM and are flushed to SD periodically to reduce SD wear.

### SDMaintenance.cpp (no .h)
Periodic SD card maintenance tasks: re-scan for new/deleted files, purge old index caches, verify filesystem health. Called from `RunSD` policy director.