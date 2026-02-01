# SD Card Root Files

> Version: 251230A | Updated: 2025-12-30

This folder contains all files that should be copied to the SD card root for the Kwal26 installation.

## Directory Structure

```
sdroot/
├── index.html              # Main web interface HTML
├── styles.css              # CSS styling for web interface
├── kwal.js                 # BUILT JavaScript (do not edit!)
├── version.txt             # Firmware version identifier
├── ping.wav                # Distance sensor feedback audio
├── calendar.csv            # Calendar events with themes
├── light_patterns.csv      # LED pattern definitions
├── light_colors.csv        # LED color palette definitions
├── theme_boxes.csv         # Theme-to-audio/visual mapping
├── audioShifts.csv         # Audio probability shifts per theme
├── colorsShifts.csv        # Color probability shifts per theme
├── patternShifts.csv       # Pattern probability shifts per theme
├── globals.csv             # Runtime globals override
├── ledmap.bin              # LED matrix mapping
├── System Volume Information/  # Dummy folder (see below)
└── webgui-src/             # JavaScript source files
```

## System Volume Information

This is a **dummy folder** that must exist in sdroot. When copying sdroot to an SD card,
Windows creates a "System Volume Information" folder automatically. If this folder is
tracked in git, the SD card copy will be consistent and git status stays clean.

**Do not delete this folder.**

## Configuration CSV Files

| File | Purpose | Format |
|------|---------|--------|
| `calendar.csv` | Daily events/themes | date;theme_box_id;... |
| `light_patterns.csv` | LED animation definitions | 16 columns, semicolon-delimited |
| `light_colors.csv` | Color palette entries | id;name;rgb1_hex;rgb2_hex |
| `theme_boxes.csv` | Theme box configuration | theme_box_id;name;audio_dirs;... |
| `audioShifts.csv` | Per-theme audio weights | theme_id;dir_weights... |
| `colorsShifts.csv` | Per-theme color weights | theme_id;color_weights... |
| `patternShifts.csv` | Per-theme pattern weights | theme_id;pattern_weights... |

## webgui-src/ Subfolder

**Source files for the modular web interface.**

```
webgui-src/
├── build.ps1           # Build script: concatenates JS → kwal.js
├── WEBGUI_CONTRACT.md  # API contract documentation
└── js/                 # Modular JavaScript sources
    ├── namespace.js    # Global namespace definition
    ├── state.js        # Application state management
    ├── audio.js        # Audio panel controls
    ├── brightness.js   # Brightness slider
    ├── colors.js       # Color selection
    ├── pattern.js      # Pattern selection
    ├── modal.js        # Modal dialogs
    ├── ota.js          # OTA update UI
    ├── sd.js           # SD card file browser
    ├── sse.js          # Server-Sent Events handler
    ├── status.js       # Status display
    ├── health.js       # Health/diagnostics
    └── main.js         # Entry point, event binding
```

### Build Process

**NEVER edit `sdroot/kwal.js` directly!**

1. Edit source files in `webgui-src/js/`
2. Run `.\build_js.ps1` from project root (or `.\build.ps1` inside webgui-src/)
3. Script concatenates all JS files in correct order → `sdroot/kwal.js`
4. Upload to SD card with `.\upload_web.ps1`

See [WEBGUI_CONTRACT.md](webgui-src/WEBGUI_CONTRACT.md) for API specifications.

## Deployment

### Upload to SD Card

Use the project script:
```powershell
.\upload_web.ps1
```

Or manually copy all files (except `webgui-src/`) to SD card root.

### Version Synchronization

1. Update `version.txt` with new version string
2. Update `?v=` cache-buster in `index.html` script/style tags
3. Update `APP_BUILD_INFO.version` in `webgui-src/js/main.js`
4. Rebuild with `build.ps1`

## File Ownership

| Files | Edited By | Deployed To |
|-------|-----------|-------------|
| CSV configs | User/Excel | SD card |
| `webgui-src/js/*.js` | Developer | Build → kwal.js |
| `kwal.js` | **NEVER** (generated) | SD card |
| `index.html`, `styles.css` | Developer | SD card |
