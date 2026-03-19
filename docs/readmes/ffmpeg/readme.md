# FFmpeg Audio Processing

> Version: 260319A | Updated: 2026-03-19

Scripts for converting MP3 files to the SD card format required by Kwal27.

## Why?

The ESP32 plays MP3s directly from SD. For reliable playback and correct duration calculation:

| Requirement | Reason |
|-------------|--------|
| **Mono** | Halves file size, ESP32 has only 1 speaker |
| **128 kbps** | Fixed bitrate for duration calculation: `ms = filesize / 16` |
| **Headless** | No ID3 tags, no Xing header — no surprises during seek |
| **Normalized** | Consistent volume (-16 LUFS) across all audio |

Long audio (stories, songs) must be split:
- Max 101 files per folder
- Short segments give variation in playback

## SD Card Structure

```
/NNN/           Folders 001..200 (max 200)
  MMM.mp3       Files 001.mp3..101.mp3 (max 101 per folder)
/000/           Special: TTS words
  001.mp3..101.mp3
```

## Scripts

| Script | Purpose |
|--------|---------|
| `norm.bat` | Convert MP3s to SD format **without** splitting |
| `normsplit.bat` | Convert and **split** MP3s into segments |
| `prep.bat` | Prepare raw audio files for processing |
| `prep_mp3.ps1` | PowerShell MP3 preparation script |
| `tosd.bat` | Copy processed files to SD card structure |
| `README.txt` | Additional usage notes |

### `norm.bat`
Converts MP3s to SD format without splitting.

```
norm.bat "D:\input\folder"
```
- Recursive (preserves folder structure)
- Output in `input\folder\converted\`
- Suitable for short audio (TTS words, sound effects)

### `normsplit.bat`
Converts and splits MP3s into segments.

```
normsplit.bat "D:\input\folder"           # 30s segments (default)
normsplit.bat "D:\input\folder" 45        # 45s segments
```
- Recursive (preserves folder structure)
- Output in `input\folder\split\`
- Suitable for long audio (stories, music)
- Filenames: `original_00000.mp3`, `original_00001.mp3`, ...

## FFmpeg Parameters

All scripts use the same conversion:

```
-af "loudnorm=I=-16:TP=-1.5:LRA=11"   # Loudness normalization
-ac 1                                  # Mono
-ar 44100                              # Sample rate
-c:a libmp3lame -b:a 128k              # MP3 128 kbps CBR
-write_xing 0                          # No Xing header
-map_metadata -1                       # Strip metadata
-id3v2_version 0 -write_id3v1 0        # No ID3 tags
```

## Workflow

```
1. Source audio (TTS, music, etc.)
          |
          v
2. norm.bat or normsplit.bat
          |
          v
3. Copy to sdMP3/NNN/
          |
          v
4. Rebuild SD index (firmware does this at boot)
```

## SD Card Maintenance

The firmware maintains index files for fast lookup. When changing files, delete the relevant index so it rebuilds at boot.

| Action on SD | Delete |
|-------------|--------|
| Add/remove MP3 in `/xxx/` | `/xxx/.files_dir` |
| Add/remove MP3 in `/000/` | `/000/.words_dir` |
| Add new directory `/xxx/` | Nothing (auto-scanned) |
| Multiple directories changed | Each `.files_dir` or just `.root_dirs` |
| Full rescan | Delete `.root_dirs` |

**Note:** Never delete the MP3 files themselves, only the `.files_dir` / `.words_dir` / `.root_dirs` indexes.

## Installatie

FFmpeg moet beschikbaar zijn:
- `ffmpeg\bin\ffmpeg.exe` (relatief pad), of
- `ffmpeg` in system PATH

Download: https://www.gyan.dev/ffmpeg/builds/
