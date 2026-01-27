# FFmpeg Audio Processing

Scripts voor het converteren van MP3's naar het SD-kaart formaat van Kwal26.

## Waarom?

De ESP32 speelt MP3's direct van SD. Om betrouwbare playback en correcte duratie-berekening te krijgen:

| Vereiste | Reden |
|----------|-------|
| **Mono** | Halveert bestandsgrootte, ESP32 heeft maar 1 speaker |
| **128 kbps** | Vaste bitrate voor duratie-berekening: `ms = filesize / 16` |
| **Headless** | Geen ID3 tags, geen Xing header → geen verrassingen bij seek |
| **Genormaliseerd** | Consistente volume (-16 LUFS) over alle audio |

Lange audio (verhalen, liedjes) moet gesplit worden:
- Max 101 bestanden per folder
- Korte segmenten geven variatie in playback

## SD-kaart Structuur

```
/NNN/           Folders 001..200 (max 200)
  MMM.mp3       Files 001.mp3..101.mp3 (max 101 per folder)
/000/           Speciaal: TTS woorden
  001.mp3..101.mp3
```

## Scripts

### `norm.bat`
Converteert MP3's naar SD-formaat **zonder** te splitten.

```
norm.bat "D:\input\folder"
```

- Recursief (behoudt mapstructuur)
- Output in `input\folder\converted\`
- Geschikt voor korte audio (TTS woorden, geluidseffecten)

### `normsplit.bat`
Converteert én **split** MP3's in segmenten.

```
normsplit.bat "D:\input\folder"           # 30s segmenten (default)
normsplit.bat "D:\input\folder" 45        # 45s segmenten
```

- Recursief (behoudt mapstructuur)
- Output in `input\folder\split\`
- Geschikt voor lange audio (verhalen, muziek)
- Bestandsnamen: `origineel_00000.mp3`, `origineel_00001.mp3`, ...

## FFmpeg Parameters

Beide scripts gebruiken dezelfde conversie:

```
-af "loudnorm=I=-16:TP=-1.5:LRA=11"   # Loudness normalisatie
-ac 1                                  # Mono
-ar 44100                              # Sample rate
-c:a libmp3lame -b:a 128k              # MP3 128 kbps CBR
-write_xing 0                          # Geen Xing header
-map_metadata -1                       # Strip metadata
-id3v2_version 0 -write_id3v1 0        # Geen ID3 tags
```

## Workflow

```
1. Bron audio (TTS, muziek, etc.)
          │
          ▼
2. norm.bat of normsplit.bat
          │
          ▼
3. Kopie naar sdMP3/NNN/
          │
          ▼
4. Rebuild SD index (firmware doet dit bij boot)
```

## SD-kaart Onderhoud

De firmware houdt index-bestanden bij voor snelle lookup. Bij wijzigingen moet je
de juiste index verwijderen zodat die opnieuw wordt opgebouwd bij boot.

| Actie op SD | Te verwijderen |
|-------------|----------------|
| MP3 toevoegen/verwijderen in `/xxx/` | `/xxx/.files_dir` |
| MP3 toevoegen/verwijderen in `/000/` | `/000/.words_dir` |
| Nieuwe directory `/xxx/` toevoegen | Niets (automatisch gescand) |
| Meerdere directories gewijzigd | Elke `.files_dir` óf gewoon `.root_dirs` |
| Alles opnieuw scannen | `.root_dirs` verwijderen |

**Let op:** Verwijder nooit de MP3's zelf, alleen de `.files_dir` / `.words_dir` / `.root_dirs` indexen.

## Installatie

FFmpeg moet beschikbaar zijn:
- `ffmpeg\bin\ffmpeg.exe` (relatief pad), of
- `ffmpeg` in system PATH

Download: https://www.gyan.dev/ffmpeg/builds/
