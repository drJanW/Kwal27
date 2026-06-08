# Handoff — DemoRun 260608F

## Huidige staat
- Firmware: **260608F** op MARMER (192.168.2.188) — gecompileerd, geüpload, getest
- Git: D+E committed als 260608C, F staat UNCOMMITTED (bump + 6 fixes)
- Volgende versie wordt **260608G**

## Wat werkt in 260608F
- Timer-chain doorloopt alle 17 chapters zonder stalls ✓
- Audio gap TTS→muziek weg (~5s, was 14s) ✓
- Wild&extreem (ch4, CF_RING_SCENE=0x40) speelt audio/056 ✓
- TV sim (ch15, CF_TV_START) speelt TTS aankondiging eerst, dan 20s tvMode ✓
- Live TTS (086-090) gegenereerd: Bram, rate=-1, voice index=1 ✓
- CF_DYN_COLOR werkt: temp→Sunny Yellow, zon→Sunrise Pink ✓
- Demo eindigt met schone reboot ✓

## Twee resterende problemen

### #1 Witte flitsen (ch1, ttsFile=1) — onbevestigd
- `startFlashSequence(5)` roept `cb_demoFlash()` **direct** aan (geen timer voor eerste vuur)
- Dit is ook punt #7 in repair_fuck.txt: architectuurschending
- `applyBrightness()` loopt elke 50ms en dempt brightness via audioLevel — kan flitsen maskeren
- Fix: `timers.create(1, 1, cb_demoFlash)` als kickoff i.p.v. directe call
- Geen log-bewijs dat flitsen niet werken — visueel testen vereist

### #4 Sunrise nooit gefetched voor demo
- Boot: 30s one-shot `cb_fetchSunrise` (token=2) toegevoegd in 260608F
- Demo start T+2min28s na boot in test → sunrise fetch was nog niet klaar
- `cb_fetchSunrise` heeft guard: `if (isSentencePlaying()) return` — boot-TTS blokkeert het
- Boot-TTS duurt 6.6s, 30s timer start daarna → sunrise fetch rond T+37s
- Demo startte T+2min28s → genoeg tijd, maar log toont `ch19 skip` → fetch faalde of gaf 0 terug
- Diagnose: check of sunrise API bereikbaar is, en of `cb_fetchSunrise` `[Fetch]` logt bij succes
- **Voeg log toe**: in `cb_fetchSunrise` succes-pad: `PF("[Fetch] Sunrise: %u:%u → %u:%u\n", rh, rm, sh, sm)`

## repair_fuck.txt — open punten
Bestand: `docs/todos/repair_fuck.txt`
1. `estimateTtsMs()` doet SD.open() zonder busy-wait retry — risico bij druk SD
3. `cb_demoThunder()` — dead code (geen chapter met ttsFile==7), verwijderen
4. `CF_DYN_PATTERN` flag — gedefinieerd, nooit gebruikt, verwijderen
5. `scaledAudioMs()` — handmatige if-clamp, gebruik `clamp()` uit MathUtils
6. NATURAL_TOTAL_MS comment zegt "24-chapter", moet "17-chapter"
7. `startFlashSequence()` — directe call i.p.v. timer kickoff (= ook fix voor #1)

## Architectuur DemoRun
Zie `/memories/repo/demorun.md` — volledig gedocumenteerd.

Timer chain:
```
cb_demoChapterStart → cb_demoChapterLight → cb_demoChapterAudio → cb_demoChapterDone → (1ms) → cb_demoChapterStart
```
Uitzonderingen:
- CF_TV_START: TTS spelen → `cb_demoTvEnter` (delay) → `enterTvMode` → `cb_demoChapterDone`
- CF_RING_SCENE: infinite ring timer in Light, cancel in Done

## Regels voor volgende sessie
- Versie bump EERST: `lib/Globals/Globals.h` lijn 17 → `260608G`
- Nooit `pio` of `deploy.ps1` uitvoeren — zeg "Compileer Versie 260608G"
- Nooit uploaden naar 192.168.2.188 — alleen user mag dat
- Nooit `upload_csv.ps1`
- Na elke edit: `get_errors` checken

## Wat te doen in volgende sessie
1. Commit 260608F: `git add -A; git commit -m "260608D-F: timer chain rewrite, sunrise boot fetch, audio gap fix, TV sim TTS fix"`
2. Fix repair_fuck.txt punten 3+4+5+6 (dead code + style) → 260608G
3. Fix #7 (startFlashSequence timer kickoff) → test flitsen
4. Diagnose #4 (sunrise): voeg success-log toe in cb_fetchSunrise
