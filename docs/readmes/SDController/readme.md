# SDController

> Version: 260319A | Updated: 2026-03-19

RAM-efficient, index-driven MP3 file selection with score-based weighted random picking. No scan loops, 100% consistent indexing.

## Files

| File | Version | Purpose |
|------|---------|---------|
| `SDController.h/.cpp` | 260227B | SD card control: init, locking, index rebuild, file I/O |
| `SDVoting.h/.cpp` | 260316I | Audio fragment voting system with score tracking per file |
| `version.txt` | — | SD layout version identifier |

## SD Card File Structure

### Directories
- Only numbered subdirs allowed: `/001/`, `/002/`, ... `/NNN/` (max `SD_MAX_DIRS`, e.g. 200)
- `/000/` is reserved for TTS words (special handling)
- No other directories in root

### MP3 Files
- Filenames must be exactly `XXX.mp3` (e.g. `007.mp3`)
- Max `SD_MAX_FILES_PER_SUBDIR` (e.g. 101) per directory
- Other extensions/names/hidden files are ignored
- Only files with score > 0 are valid for selection

### Root Files
- `/.root_dirs` — binary index for subdirectories
- `/version.txt` — SD layout version (first line only)
- No loose MP3s in root

### Per-Subdir Files
- `/NNN/.files_dir` — binary index for files in that subdir
- `/000/.words_dir` — binary index with TTS word durations

## Index File Formats

### `.root_dirs`
Location: `/.root_dirs` (always in root)

`DirEntry` per directory (4 bytes each):
| Offset | Field | Type | Description |
|--------|-------|------|-------------|
| 0 | fileCount | uint16_t | Number of valid MP3s (score > 0) |
| 2 | totalScore | uint16_t | Sum of all scores in this subdir |

Total size: `SD_MAX_DIRS x 4 bytes` = 200 x 4 = **800 bytes**

### `.files_dir`
Location: `/NNN/.files_dir` (per subdir)

`FileEntry` per file slot (4 bytes each):
| Offset | Field | Type | Description |
|--------|-------|------|-------------|
| 0 | sizeKb | uint16_t | File size in kB |
| 2 | score | uint8_t | 0 = invalid, 1..200 = valid |
| 3 | reserved | uint8_t | Reserved (padding) |

Total size: `SD_MAX_FILES_PER_SUBDIR x 4 bytes` = 101 x 4 = **404 bytes**

### `.words_dir`
Location: `/000/.words_dir` (TTS directory only)

`uint16_t durationMs` per word slot (2 bytes each):
| Offset | Field | Type | Description |
|--------|-------|------|-------------|
| 0 | durationMs | uint16_t | Estimated play duration in milliseconds |

Total size: `SD_MAX_FILES_PER_SUBDIR x 2 bytes` = 101 x 2 = **202 bytes**

Duration formula: `durationMs = (sizeBytes x 5826) / 100000`

## API

### SDController (all static)
```cpp
bool begin(uint8_t csPin);
void setReady(bool ready);
bool checkPresent();
void lockSD() / unlockSD();
void rebuildIndex();
void scanDirectory(uint8_t dir_num);
void syncDirectory(uint8_t dir_num);
void rebuildWordsIndex();
void updateHighestDirNum();
uint8_t getHighestDirNum();
bool readDirEntry(uint8_t dir_num, DirEntry* entry);
bool writeDirEntry(uint8_t dir_num, const DirEntry* entry);
bool readFileEntry(uint8_t dir_num, uint8_t file_num, FileEntry* entry);
bool writeFileEntry(uint8_t dir_num, uint8_t file_num, const FileEntry* entry);
bool fileExists(const char* fullPath);
bool writeTextFile(const char* path, const char* text);
String readTextFile(const char* path);
bool deleteFile(const char* path);
```

### SDVoting (namespace)
```cpp
uint8_t getRandomFile(uint8_t dir_num);   // Weighted random, returns 0 if none
uint8_t applyVote(uint8_t dir_num, uint8_t file_num, int8_t delta);
void saveAccumulatedVotes();
void banFile(uint8_t dir_num, uint8_t file_num);
void deleteIndexedFile(uint8_t dir_num, uint8_t file_num);
bool getCurrentPlayable(uint8_t& dirOut, uint8_t& fileOut);
void attachVoteRoute(AsyncWebServer& server);
```

### Path Helper
```cpp
const char* getMP3Path(uint8_t dirID, uint8_t fileID);
// Example: getMP3Path(114, 7) -> "/114/007.mp3"
```

## Weighted Random Selection

- Only files with score > 0 (from `.files_dir`) participate
- Only directories with `fileCount > 0` and `totalScore > 0` are considered
- Higher score = higher probability of being selected
- `SDVoting::getRandomFile(dir_num)` returns file_num (1-based), or 0 if none valid

## AudioDirector Integration

- Theme boxes reference directory ranges; only dirs with valid index entries are used
- Empty filters produce log messages: `[AudioDirector] No weighted directories available`
- Selection stops without fallback if no valid entries exist -- keep SD scores up to date

## Version Check

`/version.txt` first line is compared via `versionStringsEqual()` (ignores whitespace/CR/LF).
Mismatch -> halt, re-index required.

## Index Rebuild Rules

- Never manually modify `.files_dir`, `.root_dirs`, or `.words_dir`
- After adding/removing files: `rebuildIndex()` rebuilds everything
- Or delete the relevant index file and reboot (firmware rebuilds at boot)
- Never place `.files_dir` in root -- only in subdirs