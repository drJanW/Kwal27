# SD Card Files

Files in SD root (`/`) needed for full operation.

## Must be on SD card

| File | Description |
|------|-------------|
| `config.txt` | Device identity: name, ssid, password, ip, gateway, sensor flags |
| `version.txt` | SD index format version stamp |
| `globals.csv` | Runtime config overrides (brightness, volume, intervals) |
| `calendar.csv` | Theme schedule per date range |
| `theme_boxes.csv` | Theme→dir mapping + box colors |
| `light_patterns.csv` | LED pattern definitions |
| `light_colors.csv` | LED color palette definitions |
| `colorsShifts.csv` | Time-of-day color shift rules |
| `patternShifts.csv` | Time-of-day pattern shift rules |
| `audioShifts.csv` | Time-of-day audio volume/dir shift rules |
| `index.html` | Web interface |
| `kwal.js` | JavaScript (built from `webgui-src/js/`) |
| `styles.css` | CSS stylesheet |
| `ledmap.bin` | LED index→physical position mapping |
| `ping.wav` | Boot chime |
| `System Volume Information` | 0-byte file, voorkomt dat Windows een map aanmaakt |

## MP3 directories

```
/000/           TTS word fragments (001.mp3 .. NNN.mp3)
/001/ .. /200/  Music/ambient MP3s (max 101 per dir)
```

MP3s: mono, 128 kbps max, no ID3 tags, filenames `001.mp3`..`101.mp3`.

## Auto-generated (firmware creates, don't copy)

`/.root_dirs`, `/NNN/.files_dir`, `/000/.words_dir`, `/config/last_time.txt`

## Upload commands

```powershell
.\upload_web.ps1 192.168.2.189       # index.html, kwal.js, styles.css
.\upload_csv.ps1 189                 # all CSVs
.\sync_mp3.ps1 189                   # MP3 directories
# ledmap.bin + ping.wav: copy to SD card directly (rarely change)
```
