# TTS Queue — Pseudo Code

> Standalone feature: zelf-consumerende TTS-render-queue op SD.
> Gebruikt door demo (alle chapter-TTS), maar **niet demo-specifiek** —
> elke feature die vooraf-gerenderde TTS-mp3's nodig heeft kan regels
> toevoegen aan `/tts_todo.csv`.

Status: ontwerp. Geen real code.

---

## 1. CSV-formaat — `sdroot/tts_todo.csv`

```csv
# tts_todo.csv — TTS render-queue, self-consuming bij boot
# regel wordt na succesvolle render uit dit bestand verwijderd
# formaat: lang; voice; tempo; dir; file; tekst
# - lang  : NL (enige toegestane waarde nu)
# - voice : 0=Lotte 1=Bram 2=Daan; -1=random
# - tempo : -3..+3; 99=random
# - dir   : 1-200 (SD-dir nummer)
# - file  : 1-99
# - tekst : alles na de 5e ';', max 160 chars; geen ';' in tekst
NL; 1; -1; 150; 001; welkom bij kwal twee demo
NL; 1; -1; 150; 005; ook vandaag is het mooi weer
```

### Parse-regels
- Skip lege regels en regels die met `#` beginnen
- Strip leidende/volg-whitespace per veld
- Strip BOM (UTF-8) bij file-open
- Strip `\r` (CRLF tolerant)
- Splits eerste 5 `;` — alles na de 5e `;` is `tekst` (mag spaties en
  leestekens bevatten, behalve `;`)

---

## 2. Flow — zelf-consumerend bij boot

Self-consuming queue. Eén regel per fire. Bij elke boot pikt-ie waar-ie
gebleven was. Geen UI nodig — upload met
`upload_file.ps1 tts_todo.csv <ip>`.

```
cb_ttsTodoBoot                 (1× bij boot, 30s delay)
 ├─ /tts_todo.csv niet bestaan of leeg → klaar, exit
 └─ timers.create(SECONDS(1), 1, cb_ttsTodoNext)

cb_ttsTodoNext                 (één regel per fire)
 ├─ lees eerste niet-#/niet-lege regel uit /tts_todo.csv
 ├─ geen regels meer → log "tts queue klaar", exit (geen reschedule)
 ├─ parse fout (verkeerd aantal velden, lang ≠ NL,
 │              dir/file/voice/tempo out-of-range)
 │    → log waarschuwing met regel-inhoud
 │    → regel LATEN STAAN (jij moet hem zien om te fixen)
 │    → exit (geen reschedule — fout-regels blokkeren queue tot fix)
 ├─ PlaySentence::downloadTtsToCache(tekst, voice, tempo, dir, file)
 │    ├─ success → CSV rewrite zonder die regel
 │    │           (atomair via .tmp + rename)
 │    │           → timers.create(SECONDS(10), 1, cb_ttsTodoNext)
 │    │             [throttle: VoiceRSS rate-limit + SD-write rust]
 │    └─ fail (geen wifi, VoiceRSS down, SD-write fail)
 │           → log "tts render failed, retry next boot"
 │           → exit (regel blijft staan)
```

---

## 3. Vangrails

| Situatie | Gedrag | Reden |
|----------|--------|-------|
| Parse-fout | HARD STOP queue, regel blijft | Anders gaat typo silently voorbij |
| Network/HTTP-fout | SOFT STOP, regel blijft | Volgende boot opnieuw proberen |
| SD-write fout (rewrite) | SOFT STOP, regel blijft | Idem |
| `lang ≠ NL` | Parse-fout | EN nog niet ondersteund |
| `voice` ∉ {-1, 0, 1, 2} | Parse-fout | Onbekende stem |
| `tempo` ∉ {-3..+3, 99} | Parse-fout | VoiceRSS-bereik |
| `dir` ∉ {1..200} | Parse-fout | SD-mapnummer-bereik |
| `file` ∉ {1..99} | Parse-fout | SD-bestand-bereik |
| `tekst` leeg of >160 chars | Parse-fout | VoiceRSS-limiet |
| Bestand bestaat niet | Exit silent | Niets te doen |
| Bestand alleen comments | Exit silent | Idem |

10s throttle tussen succesvolle renders. Geen retry-counter per regel
(network-fout = next boot opnieuw, parse-fout = jij fixt handmatig).

---

## 4. Atomaire CSV-rewrite

Na succesvolle render moet de zojuist-verwerkte regel weg uit de CSV.
Veilig bij power-loss midden in de write:

```
1. open /tts_todo.csv voor read
2. open /tts_todo.tmp  voor write
3. kopieer alle regels behalve de net-verwerkte → .tmp
4. close beide
5. SD.remove("/tts_todo.csv")
6. SD.rename("/tts_todo.tmp", "/tts_todo.csv")
```

Bij crash tussen stap 5 en 6: `.tmp` bestaat, `.csv` niet → bij next
boot detecteer dit en hernoem `.tmp` → `.csv` als eerste actie in
`cb_ttsTodoBoot`.

```
cb_ttsTodoBoot start:
 ├─ als /tts_todo.tmp bestaat en /tts_todo.csv niet:
 │    → SD.rename("/tts_todo.tmp", "/tts_todo.csv")  [crash recovery]
 ├─ als /tts_todo.tmp bestaat en /tts_todo.csv ook:
 │    → SD.remove("/tts_todo.tmp")  [stale tmp, gooi weg]
 └─ ... ga verder met normale flow
```

---

## 5. Bestanden

Firmware (C++):
- [ ] `lib/RunManager/Tts/TtsTodoQueue.h`   — nieuw
- [ ] `lib/RunManager/Tts/TtsTodoQueue.cpp` — nieuw
       - `cb_ttsTodoBoot()` — boot-entry, crash-recovery, file-check
       - `cb_ttsTodoNext()` — één regel per fire
       - `parseLine()` — split + valideer + returnt struct
       - `rewriteCsvWithoutFirstActive()` — atomair via .tmp
       - `logParseError()` — uniforme foutmelding
- [ ] `lib/RunManager/RunManager.cpp`       — boot-sequencer registreert
       `cb_ttsTodoBoot` met 30s delay (na WiFi + SD klaar)

Hergebruik (geen wijzigingen):
- `lib/AudioManager/PlaySentence.cpp::downloadTtsToCache()` — bestaande
  VoiceRSS-render-functie, exact signature die `cb_ttsTodoNext` aanroept

Data (gebruiker):
- [ ] `sdroot/tts_todo.csv` — initiële batch (zie [docs/demo_program.txt](demo_program.txt))

---

## 6. Bewust GÉÉN

- **Geen web-UI voor queue-beheer** — upload via `.ps1` is genoeg
- **Geen SSE-progress** — logs zijn genoeg voor v1
- **Geen EN/andere talen** — wacht use-case 2, dan refactoren
- **Geen `Globals::demoVoice` / `demoTempo`** — per-regel in CSV is netter
- **Geen retry-counter per regel** — network = next boot, parse = jij fixt
- **Geen progress-indicator op LEDs** — render duurt 4-5 min, geen visuele
  feedback nodig (gebeurt achtergrond, gebruiker merkt niks)
- **Geen periodieke scan** — alleen bij boot. Na CSV-upload zonder reboot:
  upload tweede CSV blijft tot next boot wachten. Acceptabel.

---

## 7. Hergebruik buiten demo

Elke nieuwe feature die TTS-mp3's vooraf wil renderen voegt regels toe
aan `/tts_todo.csv` (handmatig of via een eigen mechanisme). Voorbeelden:

- Kerstboodschap: `NL; 0; -1; 160; 001; vrolijk kerstfeest`
- Mascotte uitspraken: `NL; 2; +1; 161; 001; hallo daar`
- Verjaardag-tts: `NL; 1; 0; 162; 001; gefeliciteerd met je verjaardag`

De queue is **feature-agnostisch** — kent geen "demo", "kerst" of
"mascotte". Doet alleen "lees regel → render → verwijder regel".

---

## 8. Open punten

1. **Boot-delay 30s** — genoeg voor WiFi + SD ready? Of verbind aan
   bestaande `weatherBootstrapIntervalMs` / `csvFetchWaitMs` (5-6s)?
   Voorkeur: 30s is veilig (eerste render mag wachten).
2. **Throttle 10s** — getest met VoiceRSS rate-limits? Bij fail:
   verhogen naar 15s.
3. **Max-bestand-grootte** — geen hard limit, maar 31 regels × 200 bytes
   = 6 KB, ruim binnen marges. Geen check nodig.
4. **Log-formaat** — kort houden: `[TTS-Q] render 150/001 OK` /
   `[TTS-Q] parse err line 5: lang=EN` / `[TTS-Q] queue done`.
