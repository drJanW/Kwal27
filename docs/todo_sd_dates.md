# TODO: SD file timestamps in backups

## Probleem
`download_csv.ps1` zet `LastWriteTime` van gedownloade bestanden op **NU**
(het moment van download), niet op de echte SD-mtime. Resultaat:
- Alle bestanden in `sd_downloads/` of `sd_188_check/` krijgen identieke
  timestamps na elke download
- Geen manier om vast te stellen wanneer een CSV op het device gewijzigd is
- "Welke CSV heb ik recent op .188 aangepast?" wordt onbeantwoordbaar
  zonder byte-vergelijking

## Oorzaak
- API `/api/sd/list` retourneert alleen `name` + `size`, geen mtime
- API voor single-file (`/api/sd/file?path=...`) geeft alleen file content
- PowerShell `Invoke-WebRequest -OutFile` zet timestamp op systeem-NU

## Oplossing (klein, ~30 regels totaal)

### Firmware (`lib/WebInterfaceController/routes/SdRoutes.cpp`)

1. **`routeListDir`**: voeg `mtime` toe aan JSON per entry:
   ```cpp
   payload += F(",\"mtime\":");
   payload += String(entry.getLastWrite());   // unix epoch seconds
   ```

2. **Nieuw `routeStat`** voor single-file mtime:
   ```cpp
   void routeStat(AsyncWebServerRequest *request) {
       // standaard guards (sdOk, sdBusy, path param, no ".." )
       // open file readonly
       // read getLastWrite() + size
       // return {"mtime": <epoch>, "size": <bytes>}
   }
   ```
   Registreren: `server.on("/api/sd/stat", HTTP_GET, routeStat);`

### Script (`download_csv.ps1`)

Per gedownload bestand, na `Invoke-WebRequest -OutFile`:
```powershell
try {
    $stat = Invoke-RestMethod "$baseUrl/api/sd/stat?path=/$fileName" -TimeoutSec 5
    if ($stat.mtime -gt 0) {
        (Get-Item $localPath).LastWriteTime =
            [DateTimeOffset]::FromUnixTimeSeconds($stat.mtime).LocalDateTime
    }
} catch {
    Write-Host "  (no mtime available)" -ForegroundColor DarkGray
}
```

Backwards compatible: oudere firmware zonder `/api/sd/stat` → catch slaat over.

## Vereisten
- ESP32 RTC moet correct staan (NTP-sync is al actief in WiFiBoot)
- SD-library `getLastWrite()` is standaard beschikbaar in Arduino SD.h
- FAT-filesystem houdt mtime per file bij — geen extra storage nodig

## Risico
- Nul. Read-only, geen SD-write, geen blocking. Past in bestaande
  `lockSD()/unlockSD()` flow.

## Versie-impact
- Firmware version bump (+1 letter)
- OTA naar 189 → test → OTA naar 188

## Niet in scope
- Geen bulk-API "geef alle file-mtimes in één call" — `routeListDir` heeft
  dat al na de toevoeging, en single-file `stat` dekt download-script
- Geen wijziging aan upload-richting (uploads stellen mtime niet in;
  ESP-side past sowieso huidige tijd toe na schrijven)
