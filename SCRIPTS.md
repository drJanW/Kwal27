# Kwal27 Scripts

Helper scripts for development, deployment and backup of the Kwal27 ESP32 project.

## Build & Upload

### `deploy.ps1`
Unified deployment script for HOUT and MARMER devices.
```
.\deploy.ps1 hout         # USB upload to HOUT (189)
.\deploy.ps1 hout +       # USB upload with CSV/JS
.\deploy.ps1 marmer       # OTA upload to MARMER (188)
.\deploy.ps1 marmer +     # OTA upload with CSV/JS
.\deploy.ps1 hout -SkipBuild   # Upload only (no rebuild)
```

### `ota.ps1`
Build and upload firmware via OTA (Over-The-Air update). 
Builds the `esp32_v3_ota` environment, waits for you to enable OTA mode in the web interface, then uploads.
```
.\ota.ps1           # Default IP .188
.\ota.ps1 189       # Custom last octet
```

### `trace.ps1`
Open serial monitor on specified COM port (default: COM6).
```
.\trace.ps1         # Uses COM6
.\trace.ps1 8       # Uses COM8
```

## Web Interface

### `sdroot\webgui-src\build.ps1`
Build `sdroot/kwal.js` from modular JS sources in `sdroot/webgui-src/js/`.
```
cd sdroot\webgui-src
.\build.ps1
```

### `upload_web.ps1`
Upload web files (index.html, styles.css, kwal.js) from `sdroot/` to ESP32 SD card.
```
.\upload_web.ps1
```

### `download_csv.ps1`
Download CSV configuration files from ESP32 SD card to local folder.
Falls back to known filenames if SD browser API fails.
```
.\download_csv.ps1                              # Default IP and folder
.\download_csv.ps1 -ip "192.168.2.100"        # Custom IP
.\download_csv.ps1 -dest "D:\backup\csv"        # Custom destination
```

### `upload_csv.ps1`
Upload all CSV files from `sdroot/` to ESP32 SD card.
```
.\upload_csv.ps1            # Default IP .188
.\upload_csv.ps1 189        # Custom last octet
```

## Backup & Archive

### `zip.ps1`
Create full project ZIP archive (respects .gitignore) one level up from project.
```
.\zip.ps1
```

### Automatic NAS Backup (firmware)
When a pattern or color set is created, updated, or deleted via the web interface,
the ESP32 automatically pushes the updated CSV to `csv_server.py` on the NAS
(`http://192.168.2.23:8081/api/upload`). The push is deferred by 1.5 seconds so it
does not block the web response. Rapid saves are coalesced into one push.

The NAS server checks:
- **Identical content** → skipped (no disk write)
- **Changed content** → archives previous version in `history/` before saving

Backed-up files: `light_patterns.csv`, `light_colors.csv`.
History lives at `/shares/Public/Kwal/csv/history/`.

**Requires:** `csv_server.py` running on the NAS (`.\nasstart.ps1`).

## Git Sync (NAS & GitHub)

### `opstart.ps1`
Pull nieuwste wijzigingen van NAS bij opstarten. Controleert eerst op open Excel bestanden.
```
.\opstart.ps1
```

Workflow:
1. Checkt op open Excel lock files
2. Haalt nieuwste wijzigingen van NAS remote
3. Fast-forward only (geen merge)

**Let op:** Als je lokale wijzigingen hebt, eerst `naspush.ps1` draaien!

### `naspush.ps1`
Commit en push lokale wijzigingen naar NAS.
```
.\naspush.ps1 "wat je hebt gedaan"
.\naspush.ps1 bug gefixt en getest
```

Workflow:
1. Checkt op open Excel lock files
2. Staged alle wijzigingen
3. Commit met opgegeven beschrijving
4. Pull --rebase (sync met NAS, voorkomt conflicten laptop/desktop)
5. Push naar NAS remote

### `hubpush.ps1`
Commit en push lokale wijzigingen naar GitHub.
```
.\hubpush.ps1 "wat je hebt gedaan"
.\hubpush.ps1 nieuwe feature toegevoegd
```

Workflow:
1. Checkt op open Excel lock files
2. Staged alle wijzigingen
3. Commit met opgegeven beschrijving
4. Push naar GitHub (origin)

## Utility

### `dirtree.ps1`
Generate directory tree listing (skips dot-folders like .git).
```
.\dirtree.ps1                     # Current directory
.\dirtree.ps1 lib                 # Specific folder
.\dirtree.ps1 -Files              # Include files
.\dirtree.ps1 -Out tree.txt       # Save to file
```

### `build_js.ps1`
(Compat wrapper) Runs the current WebGUI build script (`sdroot/webgui-src/build.ps1`) from project root.

### `export_chat.ps1`
Export Copilot chat JSON to readable text format.

**Chat sessions locatie:**
```
C:\Users\dr_wi\AppData\Roaming\Code\User\workspaceStorage\be836b196b7a81fa2ff8759d071982df\chatSessions\
```

**Gebruik:**
```powershell
# Lijst alle chats met titel
$sessionsPath = "C:\Users\dr_wi\AppData\Roaming\Code\User\workspaceStorage\be836b196b7a81fa2ff8759d071982df\chatSessions"
Get-ChildItem "$sessionsPath\*.json" | ForEach-Object {
    $json = Get-Content $_.FullName -Raw | ConvertFrom-Json
    Write-Host "$($_.BaseName): $($json.customTitle)"
}

# Export specifieke chat (kopieer ID uit lijst hierboven)
.\export_chat.ps1 -JsonPath "$sessionsPath\<chat-id>.json"
.\export_chat.ps1 -JsonPath "$sessionsPath\<chat-id>.json" -OutPath "docs\session.txt"
```

## Quick Reference

| Task | Command | Beschrijving |
|------|---------|--------------|
| Upload firmware (fast) | `.\go.ps1` | Upload bestaande build zonder rebuild |
| Build + OTA upload | `.\ota.ps1` | Build + draadloos uploaden via WiFi |
| Serial monitor | `.\trace.ps1` | Log output bekijken op COM poort |
| Build JS | `cd sdroot\webgui-src; .\build.ps1` | WebGUI JavaScript bundelen |
| Upload web files | `.\upload_web.ps1` | HTML/CSS/JS naar SD via HTTP |
| Upload CSVs | `.\upload_csv.ps1` | Alle CSV's van sdroot naar SD |
| Download CSVs | `.\download_csv.ps1` | CSV's van SD naar sd_downloads |
| Create ZIP backup | `.\zip.ps1` | Hele project zippen (.gitignore aware) |
| Export chat | `.\export_chat.ps1 -JsonPath x.json` | Copilot chat naar leesbaar tekstbestand |
| Start sessie | `.\opstart.ps1` | Pull van NAS (check Excel locks) |
| Push naar NAS | `.\naspush.ps1 "beschrijving"` | Commit + pull --rebase + push naar NAS |
| Push naar GitHub | `.\hubpush.ps1 "beschrijving"` | Commit + push naar GitHub |
