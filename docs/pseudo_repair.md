# pseudo_repair.md — Demo Mode reparatie

Status: ontwerp / pseudo-code. Geen code-aanpassingen tot expliciet akkoord.
Referentie: `docs/demo_program.txt` (waarheid), `lib/RunManager/Demo/DemoRun.cpp` (huidige stub).

---

## HANDOFF — voor verse Copilot-sessie (2026-06-05)

**Context:** Vorige chat werd te lang (>50 exchanges). Plan is 80% rond
maar nog niet uitgevoerd. Verse Copilot moet onderstaande open punten
zelf invullen, daarna implementeren. NIET aan gebruiker vragen — beslis
zelf en motiveer in de commit.

### Wat al besloten is (zie secties hieronder)
- Witte flits ch 1 = eigen DemoRun-renderer met `PlayLightShow(MakeSolidParams(CRGB::White/Black))` toggle
- Licht aan NA TTS klaar
- Variatie shifts via nieuwe StatusBits + CSV-regels (Optie A)
- TV-duur ch 29 = vast 4 uur
- Eén commit, versie 260606A

### Wat verse Copilot ZELF moet uitzoeken/beslissen vóór code
1. **`StatusBits.h`**: welke bit-posities (60/61) zijn echt vrij? Lees
   het bestand, kijk welke nummers nog niet bezet zijn. Niet gokken.
2. **`ShiftTable::parseStatusString()`**: lees de bestaande parse-functie.
   Kan er gewoon een `else if (name == "shiftDemoPattern") return STATUS_DEMO_PAT_SHIFT;`
   bij, of werkt het via een tabel? Pas correct in.
3. **`StatusFlags::getFullStatusBits()`**: hoe wordt die opgebouwd?
   Voeg een `static uint64_t demoOverrideBits_` toe + `setDemoBit()` API.
   OR'en in de getter.
4. **Live TTS ch 17-21**: lees `TtsTodoQueue.cpp` volledig. Bestaat er al
   een synchrone "render-nu" pad, of moet `startNow()` echt nieuw? Als
   `cb_ttsTodoBoot` direct callbar is met cancel-then-create van de timer
   op 0ms, is dat genoeg.
5. **Pre-chapter lux check**: `SensorController::ambientLux()` levert lux.
   Maar in welk veld zit de drempel per chapter? In de tabel `maxLux`
   toevoegen + bij overschrijden chapter overslaan. Lees plan §E voor
   exacte gedrag (skip vs uitstellen).
6. **Pair-drop slider-schaling**: lees plan §SLIDER-SCHALING. Pairs
   {30,31},{28,29},{21,27}... bij korte demo (audioFactor < ?) dropping.
   Implementeer als een filter-pas vóór state-machine start.
7. **Chapter-tabel zelf**: regel-voor-regel langs `docs/demo_program.txt`
   §C en de chapter-lijst. Huidige tabel in `DemoRun.cpp` is grotendeels
   fout (ch 1, 6, 7, 8, 22-29). Niet hergebruiken, opnieuw schrijven.

### Stappen

**FASE -1 — VERPLICHTE LEESLIJST (eerst, niets anders doen).**
Lees deze bestanden volledig. Niet samenvatten van de titel, écht lezen.
Citeer in FASE 0 minstens 1 regel uit elk bestand zodat gebruiker kan
checken dat je het echt gelezen hebt.

L1. `.github/copilot-instructions.md` — workspace-regels, HARD STOPS,
    TimerManager cheatsheet, code style, terminologie. ALLES hierin is
    bindend. Elke nieuwe chat verkloot timers omdat-ie dit overslaat.
L2. `docs/demo_program.txt` — spec voor deze taak (waarheid voor
    chapters, delays, pair-drop, lux-check).
L3. `docs/demo_plan.txt` — aanvullend demo-plan, lees of/hoe het
    verschilt van demo_program.txt. Bij conflict: vraag NIET aan
    gebruiker, kies demo_program.txt en log in commit.
L4. `docs/coding_tech/boot_stages.md` — wat draait wanneer, welke timers
    starten waar. Cruciaal voor `TtsTodoQueue` en demo-injectie.
L5. `docs/coding_tech/glossary_slider_semantics.md` — brightness/slider
    terminologie (jouw memory zegt het ook: lees dit vóór brightness-werk).
L6. `docs/coding_tech/fallback_policy.md` — hoe fallback werkt; demo
    mag niet door fallback ondersneeuwen.
L7. `lib/TimerManager/TimerManager.h` — citeer de API-signature.
    NOOIT `restart()` in callback voor constante interval. NOOIT
    `millis()`, `delay()`, `esp_timer`, `Ticker`.
L8. `lib/RunManager/Alert/AlertRGB.cpp` — referentie-implementatie van
    flits-renderer. Demo flits ch1 modelleert hierop.
L9. `lib/RunManager/Demo/DemoRun.cpp` — huidige (gebroken) state. Lezen
    voor je herschrijft.
L10. `lib/RunManager/Tts/TtsTodoQueue.cpp` — live-TTS pad voor ch 17-21.

**FASE 0 — bewijs dat je de repo + docs kent.**
Beantwoord onderstaande vragen met bestand:regelnummer + citaten.
Géén code-edits. Toon antwoorden aan gebruiker. Wacht op "ja".

B0. Citeer uit elk van L1..L10 één regel (10 citaten totaal) om te
    bewijzen dat je ze gelezen hebt.
B1. **StatusBits**: lees `lib/ContextController/StatusBits.h`. Welke bit-
    nummers (0..63) zijn al bezet? Welke 2 ga je gebruiken voor
    `STATUS_DEMO_PAT_SHIFT` / `STATUS_DEMO_COL_SHIFT`?
B2. **StatusFlags**: lees `lib/ContextController/StatusFlags.cpp`. Citeer
    de huidige `getFullStatusBits()` body. Waar plak je de OR met
    `demoOverrideBits_` aan?
B3. **ShiftTable parse**: lees `lib/.../ShiftTable.cpp` (zoek
    `parseStatusString` of equivalent). Citeer de huidige mapping van
    string→bit. Hoe voeg je 2 nieuwe namen toe?
B4. **TtsTodoQueue**: lees `lib/RunManager/Tts/TtsTodoQueue.cpp` volledig.
    Citeer signature van `plan()` en `cb_ttsTodoBoot`. Kan je met
    `timers.restart(0, 1, cb_ttsTodoBoot)` de boot-delay omzeilen zonder
    nieuwe API? Ja/nee + waarom. Check tegen TimerManager-regel: nooit
    restart in callback voor constante interval — geldt dat hier?
B5. **PlayLightShow / MakeSolidParams**: lees `lib/LightController/LightController.h`.
    Citeer signature. Bevestig dat `PlayLightShow(MakeSolidParams(CRGB::White))`
    een geldige call is.
B6. **SensorController::ambientLux**: lees signature. Returntype, range,
    blocking ja/nee.
B7. **RunManager::enterTvMode**: bestaat de functie? Citeer signature.
B8. **demo_program.txt §C**: citeer de exacte volgorde TTS→licht→audio
    + de delay-getallen. Bewijs dat je het plan kent.
B9. **demo_program.txt §SLIDER-SCHALING**: citeer de pair-drop regel.
    Bij welke audioFactor begint dropping?
B10. **Huidige `DemoRun.cpp` chapter-tabel ch1, ch6, ch7, ch8, ch29**:
     citeer huidige waarden. Wat moet het worden per plan?
B11. **TimerManager-overtredingen vermijden**: lees `.github/copilot-instructions.md`
     sectie "TimerManager — cheatsheet". Welk patroon gebruik je voor:
     (a) de flits-renderer ch1 (100ms aan/900ms uit, 5 cycles — interval varieert → restart)
     (b) de chapter state-machine (variabele delays per chapter → create one-shot per stap)
     (c) de lux-check (één keer per startStep0, NIET polling)
     Geen `millis()`, geen `restart()` voor constante interval.
B12. **CalendarData struct**: lees `lib/ContextController/CalendarSelector.h` of gelijkwaardige
     header. Heeft CalendarData::day een `patternId`/`colorId` veld?
     Heeft het een `next` entry met `ttsSentence` + `daysUntil` (of equivalent)?
     Heeft TimeState een `rtcTemperature` en `hasRtcTemperature` veld?
     Citeer de struct-definitie.

Eind FASE 0 = ÉÉN bericht aan gebruiker: 10 citaten (B0) + 12 antwoorden
(B1-B12). Kort en feitelijk. Slot: "Begin ik aan FASE 1?".

**FASE 1 — implementatie (pas na jouw "ja" op FASE 0).**
1. Versie bump `lib/Globals/Globals.h` → `260606A` EERST
2. `StatusBits.h` + 2 nieuwe bits
3. `StatusFlags.h/cpp` + `setDemoBit()` + OR in getter
4. `ShiftTable.cpp` parse uitbreiden
5. `sdroot/colorsShifts.csv` + 2 regels
6. `sdroot/patternShifts.csv` + 2 regels (user upload zelf)
7. `TtsTodoQueue.h/cpp` aanpassing (alleen als B4 zei dat nieuwe API
   nodig is — anders direct inline gebruiken in DemoRun)
8. `DemoRun.h/cpp` volledig herschrijven: chapter-tabel, flits-renderer,
   lux-check, pair-drop, live-TTS append, TV trigger
9. `get_errors` check, header-versies bumpen
10. Zeg "Compileer Versie 260606A". Stop.

### Verboden
- Vraag NIET aan gebruiker "welke kleur / welk pattern / welke duur".
  Alles staat in `demo_program.txt` of in besluiten hieronder.
- Geen parallel mechanisme. Bestaande API's uitbreiden.
- Geen `upload_csv.ps1` (user-only).
- Geen upload naar .188 zonder expliciet bevel.
- Geen pseudo-code dump met "akkoord?" — gewoon bouwen.

---

---

## DEFECTEN — vastgesteld uit live log (zie chat)

1. Chapter mapping fout / niet conform plan:
   - Ch 1 = `pat=28 (Fireworks)` — plan: "witte flitsen 100ms aan/900ms uit"
   - Ch 6/7/8 = `pat=31 (Some Split)` — plan: extreem/onweer/disco met verschillende patterns
   - Ch 22-28 = allemaal `pat=2 col=2` (Slow Breathing + Cool Ocean) — plan: shifts toepassen
   - Ch 29 = `pat=2 col=3` — plan: `Globals::tvMode = true`
   - Ch 17-21 = TTS uit prompt-MP3 (`150-17..21`), audio-MP3 `86-90` zijn nooit gerenderd
2. `INTER_PAUSE_MS=1500` is overal hetzelfde; plan eist:
   - `demoLightDelayMs` ≥ 1000ms na START van TTS (licht VOOR audio aan)
   - `AudioDelayMs` ≥ 2000ms na EINDE van TTS (audio NA stem klaar)
3. Geen lux-check vóór chapter (`#E` in plan).
4. Geen pair-drop bij korte demo (plan §SLIDER-SCHALING).
5. TV-mode start niet aan einde van demo.

---

## 1. Chapter table — herstellen per plan

Bestand: `lib/RunManager/Demo/DemoRun.cpp`

Velden uitbreiden met `lightDelayMs`, `audioDelayMs`, `lightDurationMs`, `maxLux`:

```cpp
struct Chapter {
    uint8_t  ttsFile;        // /150/<ttsFile>.mp3   ; 0 = skip narration
    uint8_t  audioFile;      // /150/<audioFile>.mp3 ; 0 = none
    uint16_t audioMaxMs;     // cap audio duur (scaled)
    uint8_t  patternId;      // light_patterns.csv id ; 0 = leave
    uint8_t  colorId;        // light_colors.csv id   ; 0 = leave
    uint8_t  flags;          // bitveld (SHIFT_PATTERN | SHIFT_COLORS | TV_START | LIVE_TTS)
    uint8_t  maxLux;         // 0 = no limit; otherwise skip if ambient > maxLux
};

enum ChapterFlag : uint8_t {
    CF_NONE          = 0,
    CF_SHIFT_PATTERN = 1 << 0,
    CF_SHIFT_COLORS  = 1 << 1,
    CF_TV_START      = 1 << 2,   // ch 29: enter tvMode after TTS, no audio
    CF_LIVE_TTS      = 1 << 3,   // ch 17-21: audioFile is live-rendered (086-090)
    CF_DYN_COLOR     = 1 << 4,   // ch 18/19/21: color selected at runtime
    CF_DYN_PATTERN   = 1 << 5,   // ch 21: pattern selected at runtime from calendar
};
```

Tabel (uittreksel — volledig in code):

```cpp
static const Chapter chapters[] = {
    // tts aud  audMs   pat col  flags          maxLux
    // tts  aud   audMs  pat  col  flags                         maxLux
    {  1,  51,  5000,    0,   0, CF_NONE,                           200},  // 01 witte flitsen: eigen renderer §6 (pat=0=skip)
    {  2,  52,  5000,    2,   3, CF_NONE,                           200},  // 02 welkom: Slow Breathing + Forest Green
    {  3,  53,  5000,   27,   2, CF_NONE,                           200},  // 03 blauwe ringen: Polar Lights + Cool Ocean
    {  4,  54,  5000,   27,  40, CF_NONE,                           200},  // 04 witte ringen: Polar Lights + Ice White
    {  5,  55,  5000,   27,  34, CF_NONE,                           200},  // 05 blauw-in-wit: Polar Lights + Periwinkle Blue (wit+blauw)
    {  6,  56,  8000,   31,  32, CF_NONE,                           200},  // 06 extreem zappa: Some Split + Spectrum
    {  7,  57,  8000,   31,   8, CF_NONE,                           200},  // 07 onweer: Some Split + Electric Blue
    {  8,  58,  8000,    3,   4, CF_NONE,                           200},  // 08 disco: Rapid Sparks + Royal Purple
    {  9,  59, 30000,   32,  32, CF_NONE,                           100},  // 09 spectrum: Spectrum + Spectrum
    {  0,   0,  2000,    0,   0, CF_NONE,                           200},  // 10 placeholder
    { 11,  61,  8000,   27,  41, CF_NONE,                            80},  // 11 lounge: Polar Lights + Lemon Yellow
    { 12,  62,  8000,    2,   2, CF_NONE,                            80},  // 12 ademen: Slow Breathing + Cool Ocean
    { 13,  63,  8000,   27,  25, CF_NONE,                            50},  // 13 noorderlicht: Polar Lights + Mint Fresh
    { 14,  64,  8000,    7,   1, CF_NONE,                           100},  // 14 gloed: Radiant Glow + Warm Sunset
    { 15,  65,  8000,    8,  11, CF_NONE,                            30},  // 15 sterren: Twinkling Stars + Midnight Blue
    { 16,  66,  8000,    5,  44, CF_NONE,                            20},  // 16 rust: Calm Center + Deep Space (id=44, NEW in CSV)
    { 17,  86,  3000,    5,  21, CF_LIVE_TTS,                         0},  // 17 tijd: Calm Center + Slate Gray
    { 18,  87,  3000,    7,   0, CF_LIVE_TTS|CF_DYN_COLOR,            0},  // 18 kamer-temp: Radiant Glow + dyn. op temp (§4)
    { 19,  88,  4000,    1,   0, CF_LIVE_TTS|CF_DYN_COLOR,            0},  // 19 zon: Misty Bloom + dyn. op dagdeel (§4)
    { 20,  89,  3000,    5,  10, CF_LIVE_TTS,                        50},  // 20 maan: Calm Center + Snow White (bri=maanfase, §4)
    { 21,  90,  4000,    0,   0, CF_LIVE_TTS|CF_DYN_COLOR|CF_DYN_PATTERN, 50},  // 21 kalender: dyn. pat+col uit event (§4)
    { 22,  72,  4000,    2,   2, CF_NONE,                            60},  // 22 basis1
    { 23,  73,  4000,    2,   2, CF_SHIFT_PATTERN,                   60},  // 23 patternShift
    { 24,  74,  4000,    2,   2, CF_NONE,                            60},  // 24 basis2
    { 25,  75,  4000,    2,   2, CF_SHIFT_COLORS,                    60},  // 25 colorsShift
    { 26,  76,  4000,    2,   2, CF_NONE,                            60},  // 26 basis3
    { 27,  77,  4000,    2,   2, CF_SHIFT_PATTERN|CF_SHIFT_COLORS,   60},  // 27 beideShifts
    { 28,  78,  4000,    2,   2, CF_NONE,                            60},  // 28 basis4
    { 29,   0,  5000,    0,   0, CF_TV_START,                       200},  // 29 TV-sim: TTS af, dan enterTvMode(4) + stop
    { 30,  80,  6000,   28,   5, CF_NONE,                           200},  // 30 vuurwerk: Fireworks + Sunny Yellow
    { 31,  81,  8000,    2,  10, CF_NONE,                           100},  // 31 afscheid: Slow Breathing + Snow White
};
```

NOTE Ch 1 "witte flitsen": gebruik bestaand `MakeSolidParams(CRGB::White)` /
`PlayLightShow()` mechanisme (zoals AlertRGB doet). Geen Ice White (= vaag wittig
blend `#F0F8FF`/`#AAAAFF`). Pure `CRGB::White` = `0xFFFFFF`.
Eigen DemoRun-renderer: zie §6.

`pat=0 col=0` in tabel = "leave current" — flits-renderer doet zelf `PlayLightShow()`
bypassend patroon-engine.

---

## 2. Aparte light/audio delays per chapter

Plan §C: `lightDelayMs` na start TTS, `AudioDelayMs` na einde TTS.

Vervang `INTER_PAUSE_MS` door:

```cpp
constexpr uint16_t LIGHT_DELAY_BASE_MS = 1000;  // licht aan, na start TTS
constexpr uint16_t AUDIO_DELAY_BASE_MS = 2000;  // audio aan, na einde TTS
```

State-machine wordt 3-staps per chapter:

```
Step 0 (start chapter):
    if (CF_TV_START): RunManager::enterTvMode(hours); stop demo; return;
    if (lux > ch.maxLux && ch.maxLux > 0): PF("[Demo] skip ch %u (lux %f > %u)"); advance; return;
    apply pattern (with optional shifts via ShiftTable)
    apply colors  (with optional shifts via ShiftTable)
    start TTS (live-render eerst gedaan bij start() voor CF_LIVE_TTS)
    schedule cb_demoStep1 at (LIGHT_DELAY_BASE_MS scaled)   // light wijziging zat al in Step 0;
                                                             // step1 dient nu als "audio start"
                                                             // bij scaledLight==0 direct door
    OPM: lichteffect is reeds aan; LIGHT_DELAY_BASE_MS in plan duidt op
         WACHTEN VOORDAT licht wijzigt na start TTS. Implementatie =
         pattern/colors NIET in Step 0 maar in tussenstap (zie alt hieronder).

Alt (closer to plan):
Step 0: start TTS only. schedule cb_demoLightChange at lightDelay.
cb_demoLightChange: apply pattern + colors (+ shifts).
cb_demoStep1 (audio): wait until TTS estimated done + AUDIO_DELAY → fire audio.
cb_demoStep2: wait audioMs, advance.
```

→ **BESLUIT**: licht aan `LIGHT_DELAY_BASE_MS` (≥1000ms) **na START** TTS — plan §C.
   Niet na einde TTS. Tijdlijn per chapter:
   - T=0:                   start TTS (if ttsFile>0)
   - T=LIGHT_DELAY_BASE_MS: cb_demoLightChange → apply pattern + colors + shifts
   - T=ttsDurationMs + AUDIO_DELAY_BASE_MS: cb_demoStep1 → start audio
   - T=+scaledAudioMaxMs:   cb_demoStep2 → advance
   Voor ch 10 (geen TTS): cb_demoLightChange op t=0, cb_demoStep1 direct daarna.

---

## 3. Variatie-chapters 22-28: shifts toepassen

**KEUZE: Optie A** — nieuwe statusbits + CSV-regels. Reden:
`ShiftTable` werkt via `StatusFlags::getFullStatusBits()` → multipliers.
Een parallel "manual shift" API zou anti-shortcut overtreden (zie
copilot-instructions). Bestaand mechanisme uitbreiden, niet kopiëren.

### Wijzigingen

1. `lib/ContextController/StatusBits.h` — twee nieuwe bits:
```cpp
#define STATUS_DEMO_PAT_SHIFT  60   // pak een vrij bit-positie (nu 60-63 vrij?)
#define STATUS_DEMO_COL_SHIFT  61
```

2. `lib/ContextController/StatusFlags.cpp` — `getFullStatusBits()` moet
   deze bits ook respecteren. Implementatie: shadow-variabele in DemoRun,
   `StatusFlags::getFullStatusBits()` OR'd ze in.

   Liever: nieuwe API `StatusFlags::setDemoBit(uint8_t bit, bool on)` die
   een static `demoOverrideBits_` zet. Then `getFullStatusBits()` OR met
   `demoOverrideBits_`.

3. `sdroot/colorsShifts.csv` — voeg toe (zichtbaar effect, geen subtiel):
```
shiftDemoPattern;0;0;0;0;0
shiftDemoColors;30;15;-30;-10;0
```
   (colors shift: hueA +30°, satA +15, hueB -30°, satB -10 → grote
    waarneembare verschuiving)

4. `sdroot/patternShifts.csv` — voeg toe:
```
shiftDemoPattern;-30;-20;20;0;30;0;0;0;0;25;0;0;0;0;0
shiftDemoColors;0;0;0;0;0;0;0;0;0;0;0;0;0;0;0
```
   (pattern shift: colorCycle sneller -30%, brightCycle sneller -20%,
    fadeWidth +20, gradientSpeed +30%, radiusOsc +25)

   Status-namen `shiftDemoPattern`/`shiftDemoColors` moeten parsen naar
   `STATUS_DEMO_PAT_SHIFT`/`STATUS_DEMO_COL_SHIFT` in `ShiftTable::parseStatusString()`.

5. `lib/RunManager/Demo/DemoRun.cpp` in `startStep0()`:
```cpp
StatusFlags::setDemoBit(STATUS_DEMO_PAT_SHIFT, ch.flags & CF_SHIFT_PATTERN);
StatusFlags::setDemoBit(STATUS_DEMO_COL_SHIFT, ch.flags & CF_SHIFT_COLORS);
LightRun::applyPattern(ch.patternId);  // zal shifts toepassen via StatusFlags
LightRun::applyColor(ch.colorId);
```

   En in `stop()`:
```cpp
StatusFlags::setDemoBit(STATUS_DEMO_PAT_SHIFT, false);
StatusFlags::setDemoBit(STATUS_DEMO_COL_SHIFT, false);
```

---

## 4. Chapter 17-21: live TTS bij demo-start

Plan: bij `DemoRun::start()` schrijf 5 regels naar `/tts_todo.csv`,
gebruik bestaande `TtsTodoQueue::plan()` om ze te renderen.

Probleem: `TtsTodoQueue` start na BOOT_DELAY_MS (30s) en tikt elke 10s.
Demo wil ze direct beschikbaar voor chapter 17 = ~halverwege demo (~3 min in).

Aanpak:

```cpp
namespace DemoRun {
void start() {
    if (Globals::demoActive) return;

    // Bouw 5 live-TTS regels en append aan tts_todo.csv
    appendLiveTtsLines();   // tijd, weer, zon, maan, kalender

    // Kick TtsTodoQueue NU (in plaats van bij volgende boot)
    TtsTodoQueue::startNow();   // NIEUWE API — start zonder BOOT_DELAY

    // demo daarna gewoon starten (live regels zijn klaar ruim voor ch 17)
    Globals::demoActive = true;
    ...
}
}
```

NIEUWE API in `TtsTodoQueue.h`:
```cpp
namespace TtsTodoQueue {
    void plan();      // bestaand: deferred boot scan
    void startNow();  // nieuw: start onmiddellijk (idempotent)
}
```

Implementatie: `startNow()` cancelt eventuele lopende boot-timer en roept
direct `cb_ttsTodoBoot()` aan.

### Tekst-generatie

```cpp
void appendLiveTtsLines() {
    auto& time = ContextController::time();
    char buf[200];

    // 17 tijd
    snprintf(buf, sizeof(buf), "NL; -1; 99; 150; 86; het is nu %u uur %u",
             time.hour, time.minute);
    appendCsvLine(buf);

    // 18 kamertemperatuur (RTC/sensor — NIET buiten-weer)
    // Plan: "kwal voelt de kamer" → rtcTemperature
    // Veld-naam verifiëren in FASE 0 B6 (hasRtcTemperature / rtcTemperature)
    if (time.hasRtcTemperature) {
        snprintf(buf, sizeof(buf),
            "NL; -1; 99; 150; 87; het is %.1f graden in deze kamer",
            time.rtcTemperature);
    } else {
        snprintf(buf, sizeof(buf),
            "NL; -1; 99; 150; 87; de kamertemperatuur kan ik nu niet meten");
    }
    appendCsvLine(buf);

    // 19 zon
    snprintf(buf, sizeof(buf),
        "NL; -1; 99; 150; 88; de zon kwam vanmorgen om %u uur %u op, en gaat onder om %u uur %u",
        time.sunriseHour, time.sunriseMinute,
        time.sunsetHour, time.sunsetMinute);
    appendCsvLine(buf);

    // 20 maan (fase → woord)
    const char* moonWord = moonPhaseWord(time.moonPhase);
    snprintf(buf, sizeof(buf),
        "NL; -1; 99; 150; 89; vanavond zien we een %s maan", moonWord);
    appendCsvLine(buf);

    // 21 kalender — plan: vandaag + eerstvolgende event
    // CalendarData-struct verifiëren in FASE 0 B12:
    //   heeft het cal.next.ttsSentence en cal.daysUntilNext?
    const CalendarData& cal = calendarSelector.calendarData();
    bool hasToday = cal.valid && !cal.day.ttsSentence.isEmpty();
    bool hasNext  = cal.valid && !cal.next.ttsSentence.isEmpty();  // VERIFIEER veldnaam
    if (hasToday && hasNext) {
        snprintf(buf, sizeof(buf),
            "NL; -1; 99; 150; 90; vandaag is %s - de eerstvolgende bijzondere dag is %s, over %d dagen",
            cal.day.ttsSentence.c_str(),
            cal.next.ttsSentence.c_str(),
            cal.daysUntilNext);  // VERIFIEER veldnaam
    } else if (hasNext) {
        snprintf(buf, sizeof(buf),
            "NL; -1; 99; 150; 90; vandaag is een gewone dag - de eerstvolgende bijzondere dag is %s, over %d dagen",
            cal.next.ttsSentence.c_str(),
            cal.daysUntilNext);
    } else {
        snprintf(buf, sizeof(buf),
            "NL; -1; 99; 150; 90; vandaag is een gewone dag in kwal's kalender");
    }
    appendCsvLine(buf);
}

// Dynamic color/pattern selection for CF_DYN_COLOR / CF_DYN_PATTERN chapters
// Call from startStep0() BEFORE applyPattern/applyColor when flags set.

uint8_t selectColorForTemp(float tempC) {
    // ch18: koud < 18°C = Cool Ocean, mild 18-23°C = Sunny Yellow, warm ≥23°C = Citrus Orange
    if (tempC < 18.0f) return 2;   // Cool Ocean
    if (tempC < 23.0f) return 5;   // Sunny Yellow
    return 14;                      // Citrus Orange
}

uint8_t selectColorForDaypart(const TimeState& t) {
    // ch19: dagdeel via sunrise/sunset
    int nowMins     = t.hour * 60 + t.minute;
    int sunriseMins = t.sunriseHour * 60 + t.sunriseMinute;
    int sunsetMins  = t.sunsetHour  * 60 + t.sunsetMinute;
    if (nowMins < sunriseMins)           return 11;  // Midnight Blue (nacht)
    if (nowMins < sunriseMins + 90)      return 31;  // Sunrise Pink (ochtend)
    if (nowMins < sunsetMins  - 60)      return  5;  // Sunny Yellow (dag)
    if (nowMins < sunsetMins  + 30)      return 14;  // Citrus Orange (schemering)
    return 11;                                       // Midnight Blue (nacht)
}

// Ch20 maan: Snow White (col=10) + brightness = maanfase
// moonPhaseFraction: phase 0=nieuw→donker, 0.5=vol→helder
//   fraction = (phase <= 0.5f) ? phase * 2.0f : (1.0f - phase) * 2.0f
// Brightness-API: verifieer in FASE 0 B12 (glossary_slider_semantics.md).
// Waarschuwing: applyBrightness() draait elke 50ms en overschrijft manual sets —
//   check of er een demo-brightness-override bestaat of maak er een.

// Ch21 kalender: CF_DYN_COLOR|CF_DYN_PATTERN
// CalendarData bevat mogelijk patternId + colorId per event.
// VERIFIEER in FASE 0 B12: heeft CalendarData::day een patternId/colorId veld?
// Als ja: gebruik die. Als nee: gebruik default pat=1 (Misty Bloom) col=33 (Buttercup).
// NOTE: als CalendarData GEEN pattern/color-ID heeft, dan CF_DYN_PATTERN weggooien
// en gewoon pat=1 col=33 hardcoderen voor ch21.

const char* moonPhaseWord(float phase) {
    // phase: 0=new, 0.25=first quarter, 0.5=full, 0.75=last quarter
    if (phase < 0.05f || phase > 0.95f) return "nieuwe";
    if (phase < 0.20f)                  return "wassende halve";
    if (phase < 0.30f)                  return "halve";
    if (phase < 0.45f)                  return "wassende";
    if (phase < 0.55f)                  return "volle";
    if (phase < 0.70f)                  return "afnemende";
    if (phase < 0.80f)                  return "halve afnemende";
    return "afnemende sikkel";
}
```

NOTE: bij CF_LIVE_TTS chapters: als `/150/086.mp3` niet bestaat na 30s
wachten → skip chapter. Plan #E.

---

## 5. Chapter 29: TV-mode triggeren

In `cb_demoStep2` voor laatste-TTS chapter:

```cpp
if (ch.flags & CF_TV_START) {
    PL("[Demo] TV mode trigger (4u)");
    DemoRun::stop();   // schoon op
    RunManager::enterTvMode(4);   // vast 4u
    return;
}
```

---

## 6. Witte flitsen (ch 1)

Eigen kleine DemoRun-renderer die `PlayLightShow(MakeSolidParams(c))`
afwisselt — identiek patroon als AlertRGB maar zonder error-context.

```cpp
static bool   flashOn_ = false;
static uint8_t flashRemaining_ = 0;

void cb_demoFlash() {
    if (flashRemaining_ == 0) return;  // gestopt via stop()
    flashOn_ = !flashOn_;
    if (flashOn_) {
        PlayLightShow(MakeSolidParams(CRGB::White));
        timers.restart(100, 1, cb_demoFlash);  // 100ms aan — restart: interval varieert (100/900)
    } else {
        PlayLightShow(MakeSolidParams(CRGB::Black));
        flashRemaining_--;
        if (flashRemaining_ > 0) {
            timers.restart(900, 1, cb_demoFlash);  // 900ms uit — restart: interval varieert
        }
    }
}

void startFlashSequence(uint8_t cycles) {
    flashRemaining_ = cycles;
    flashOn_ = false;
    cb_demoFlash();
}
```

In `startStep0()` voor chapter 1: `startFlashSequence(5)` (5 flitsen à 1s = 5s
totaal, matcht audioMaxMs=5000).

**Geen** nieuwe pattern-rij in CSV. **Geen** Ice White. Pure `CRGB::White`.

---

## 7. Pair-drop bij korte demo

Plan §SLIDER-SCHALING geeft exacte drop-volgorde:
```
-1: drop 31 + 30
-2: drop 29 + 28
-3: drop 27 + 21
...
```

Implementatie:

```cpp
static const uint8_t dropPairs[][2] = {
    {30, 31}, {28, 29}, {21, 27}, {20, 26}, {19, 25},
    {18, 24}, {17, 23}, {16, 22}, ...
};

// Bij start():
//   bereken total natural = sum(audioMaxMs + estTtsMs + delays)
//   while (total > demoTotalMs && dropIdx < dropPairsCount):
//       mark chapters[dropPairs[dropIdx][0]-1].skip = true
//       mark chapters[dropPairs[dropIdx][1]-1].skip = true
//       total -= ch[a].naturalMs + ch[b].naturalMs
//       dropIdx++
//   audioFactor_ = demoTotalMs / total;
```

Skip in Step 0: als `ch.skip == true` → advance.

→ Pseudo OK, implementeren straightforward.

---

## 8. Pre-chapter lux check (plan #E)

```cpp
void startStep0() {
    const Chapter& ch = chapters[Globals::demoChapterIdx];
    if (ch.maxLux > 0) {
        float lux = SensorController::ambientLux();
        if (lux > ch.maxLux) {
            PF("[Demo] skip ch %u (lux %.0f > %u)\n",
               Globals::demoChapterIdx + 1, lux, ch.maxLux);
            Globals::demoChapterIdx++;
            startStep0();
            return;
        }
    }
    // ... rest van chapter
}
```

Straightforward, `SensorController::ambientLux()` bestaat (zie LightRun::cb_measureLux).

---

## 9. Versie + commit

- `Globals.h` line 17: `FIRMWARE_VERSION_CODE "260606A"` (nieuwe datum bij implementatie)
- Per gewijzigd bestand: @version en @date header
- Compileer-bevel aan gebruiker: "Compileer Versie 260606A"

---

## BESLUITEN — vastgelegd (geen open vragen meer)

1. **Witte flits ch 1**: eigen DemoRun-renderer met `PlayLightShow(MakeSolidParams(CRGB::White/Black))` toggle. `restart()` voor variërende intervals. Geen CSV-edit, geen Ice White.
2. **Licht delay**: licht aan `LIGHT_DELAY_BASE_MS` (≥1000ms) **na START** TTS — niet na einde. Plan §C.
3. **Variatie shifts**: Optie A — nieuwe StatusBits `STATUS_DEMO_PAT_SHIFT` + `STATUS_DEMO_COL_SHIFT`, regels in beide shift-CSV's, `StatusFlags::setDemoBit()` API.
4. **TV-duur ch 29**: vast 4 uur.
5. **Volgorde**: één commit.
6. **Deep Space (ch 16)**: nieuw in `sdroot/light_colors.csv`: `44;Deep Space;#060615;#02020A;1.0000` (cnf=1.0, needs calibration). User upload.
7. **Ch 18 TTS**: kamertemperatuur (RTC), NIET buiten-weer. Veld `time.rtcTemperature` / `time.hasRtcTemperature` — verifieer in FASE 0.
8. **Ch 21 TTS**: volledig (vandaag + next event + daysUntil). CalendarData-struct verifiëren in FASE 0 B12.

## VOLGORDE VAN WIJZIGINGEN

1. `StatusBits.h` + bits 60/61
2. `StatusFlags.h/cpp` + `setDemoBit()` API
3. `ShiftTable.cpp` + `parseStatusString()` voor de twee nieuwe namen
4. `sdroot/light_colors.csv` — regel 44 toevoegen: `44;Deep Space;#060615;#02020A;1.0000` (user upload; cnf calibratie later)
5. `colorsShifts.csv` + 2 regels (user upload)
6. `patternShifts.csv` + 2 regels (user upload)
7. `TtsTodoQueue.h/cpp` + `startNow()` API
8. `DemoRun.h/cpp` — chapter-tabel, flits-renderer, live-TTS append, dyn. color/pattern, lux-check, pair-drop, TV trigger
9. `Globals.h` — versie naar 260606A, nieuwe delay-constanten
10. Compileer-bevel aan gebruiker.

CSV-uploads zijn USER-werk per hard rule. Ik wijzig de CSVs in repo,
zeg dan welke files re-uploaden naar .188 voor het werkt.
