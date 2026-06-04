# Demo Mode — Pseudo Code (alle aan te passen bestanden)

Status: ontwerp. Begeleidt `demo_plan.txt`. Geen real code.

---

## 1. `lib/Globals/Globals.h`  (al gedaan: demoDir)

```cpp
// Demo
inline static uint8_t  demoDir         = 150U;     // SD dir met demo MP3's
inline static bool     demoActive      = false;    // true = orchestratie onderdrukt context/kalender
inline static uint32_t demoTotalMs     = 90000U;   // duur uit gele slider (ms)
inline static uint8_t  demoChapterIdx  = 0U;       // huidige chapter (0..N-1)
inline static uint8_t  demoPhase       = 0U;       // 0 = TTS (A), 1 = Show (B)
```

---

## 2. `lib/Globals/Globals.cpp`  (CSV parser)

```cpp
// Already added: demoDir handler
// Geen nieuwe CSV-keys nodig voor demoActive/demoTotalMs (alleen runtime).
```

---

## 3. `lib/RunManager/Demo/DemoRun.h` (nieuw)

```cpp
namespace DemoRun {
    void start(uint32_t totalDurationMs);    // entry vanuit web handler
    void stop();                              // forceer stop
    bool isActive();                          // status query
    // intern: chapter-tabel, cb_demoNextPhase
}
```

---

## 4. `lib/RunManager/Demo/DemoRun.cpp` (nieuw)

```cpp
struct DemoChapter {
    uint8_t  ttsFileIdx;     // MP3 in /150/ voor fase A (TTS)
    uint8_t  showFileIdx;    // MP3 in /150/ voor fase B (muziek/sfx), 0 = stil
    uint16_t showDurationMs; // duur fase B (fase A duurt = TTS file lengte)
    uint8_t  patternId;      // pattern voor zowel A als B
    uint8_t  colorsId;       // colors  voor zowel A als B
};

// Chapter-tabel: vast, in code
static const DemoChapter chapters[] = {
    // chap1 opening    TTS  show  showMs   pat  col
    {  1,   2,   2000,   50,  50 },  // 1. opening + bel/sirene flits
    {  3,   4,   8000,   51,  51 },  // 2. layered glow
    {  5,   6,  10000,   52,  52 },  // 3. pride/plasma
    {  7,   8,   8000,   53,  53 },  // 4. pinwheel
    {  9,  10,   6000,   54,  54 },  // 5. confetti
    { 11,  12,   6000,   55,  55 },  // 6. disco/strobe
    { 13,  14,   8000,   56,  56 },  // 7. heartbeat/afsluiting
};
static const uint8_t CHAPTER_COUNT = sizeof(chapters)/sizeof(chapters[0]);

void start(uint32_t totalDurationMs) {
    if (Globals::demoActive) return;
    Globals::demoActive     = true;
    Globals::demoTotalMs    = totalDurationMs;
    Globals::demoChapterIdx = 0;
    Globals::demoPhase      = 0; // fase A start

    // Safety: één-shot auto-stop timer (slider-duur)
    timers.create(totalDurationMs, 1, cb_demoEnd);

    // Begin eerste chapter, fase A (TTS)
    cb_demoStartPhase();
}

void stop() {
    Globals::demoActive = false;
    timers.cancel(cb_demoNextPhase);
    timers.cancel(cb_demoEnd);
    PlayAudioFragment::stop(500);
    // RunManager pakt de normale orchestratie weer op
}

bool isActive() { return Globals::demoActive; }

// Start huidige (chapterIdx, phase): zet pattern+colors, speel audio,
// plan volgende fase.
static void cb_demoStartPhase() {
    if (!Globals::demoActive) return;
    if (Globals::demoChapterIdx >= CHAPTER_COUNT) {
        stop();
        return;
    }
    const DemoChapter& ch = chapters[Globals::demoChapterIdx];

    // Pattern + colors gelijk voor A en B (volgens demo_plan: fase A
    // toont alvast hetzelfde effect als fase B).
    RunManager::requestPatternId(ch.patternId, "demo");
    RunManager::requestColorsId (ch.colorsId,  "demo");

    AudioFragment frag = {};
    frag.dirIndex  = Globals::demoDir;            // 150
    frag.fileIndex = (Globals::demoPhase == 0) ? ch.ttsFileIdx
                                               : ch.showFileIdx;
    frag.startMs   = 0;
    frag.fadeMs    = 0;
    strncpy(frag.source, "demo", sizeof(frag.source));

    uint32_t nextMs;
    if (Globals::demoPhase == 0) {
        // Fase A: TTS, duur = file lengte (PlayAudioFragment kan dit
        // afleiden uit file size, of we lezen vooraf de duur uit)
        frag.durationMs = 0;  // 0 = play tot einde
        nextMs = getMp3DurationMs(Globals::demoDir, ch.ttsFileIdx);
    } else {
        // Fase B: show audio (muziek/sfx), duur = showDurationMs
        frag.durationMs = ch.showDurationMs;
        nextMs = ch.showDurationMs;
    }
    if (frag.fileIndex > 0) PlayAudioFragment::start(frag);

    timers.create(nextMs, 1, cb_demoNextPhase);
}

// Schakel naar volgende fase, of volgend chapter
static void cb_demoNextPhase() {
    if (!Globals::demoActive) return;
    if (Globals::demoPhase == 0) {
        Globals::demoPhase = 1;          // fase A → B
    } else {
        Globals::demoPhase = 0;          // fase B → volgend chapter, fase A
        Globals::demoChapterIdx++;
    }
    cb_demoStartPhase();
}

// Slider-tijd op, of na laatste chapter
static void cb_demoEnd() {
    stop();
}
```

---

## 5. `lib/RunManager/RunManager.cpp`  (orchestration guard)

```cpp
// In de bestaande context/kalender-driven pattern/colors switch callbacks:
void cb_calendarTick() {
    if (Globals::demoActive) return;   // demo heeft het stuur
    // ... bestaande logica ...
}

void cb_contextEvaluate() {
    if (Globals::demoActive) return;
    // ... bestaande logica ...
}

// Idem voor audio-fragment scheduler:
void cb_scheduleNextAudio() {
    if (Globals::demoActive) return;
    // ... bestaande logica ...
}
```

(Dezelfde no-op guard die TvMode al gebruikt.)

---

## 6. `lib/WebInterfaceController/Routes.cpp` (of waar routes geregistreerd)

```cpp
// Bestaande pattern:
// server.on("/api/tvmode", HTTP_GET, routeTvMode);

server.on("/api/demomode", HTTP_GET, routeDemoMode);
```

---

## 7. `lib/WebInterfaceController/Handlers/DemoMode.cpp` (nieuw)

```cpp
// Web-handler: ALLEEN state setten, geen SD/network I/O hier.
void routeDemoMode(AsyncWebServerRequest* request) {
    bool active = false;
    uint32_t durationMs = 90000;  // default 90s

    if (request->hasParam("active")) {
        active = (request->getParam("active")->value() == "1");
    }
    if (request->hasParam("duration")) {
        durationMs = strtoul(request->getParam("duration")->value().c_str(),
                             nullptr, 10);
    }

    // Defer actual work to timer callback (memory-only here)
    Globals::demoRequestActive = active;
    Globals::demoRequestDurMs  = durationMs;
    timers.create(0, 1, cb_demoApplyRequest);  // run ASAP outside web ctx

    request->send(200, "application/json", "{\"ok\":true}");
}

// Callback (in DemoRun.cpp of RunManager): doet het echte werk
void cb_demoApplyRequest() {
    if (Globals::demoRequestActive) DemoRun::start(Globals::demoRequestDurMs);
    else                            DemoRun::stop();
}
```

---

## 8. `lib/WebInterfaceController/WebGuiStatus.cpp`  (status broadcast)

```cpp
// In JSON status response (de bestaande /api/status of equivalent):
json += F(",\"demoActive\":");
json += Globals::demoActive ? F("true") : F("false");
json += F(",\"demoChapter\":");
json += Globals::demoChapterIdx;
```

(WebGUI gebruikt dit om de knop te highlighten en evt. progress te tonen.)

---

## 9. `sdroot/webgui-src/js/controls.js` (of waar knoppen gedefinieerd)

```javascript
// Nieuwe knop "Demo" (geel/feestelijk) + duur-slider
// HTML staat in sdroot/index.html (direct te editen, geen build nodig)

function startDemo() {
    const durationSec = document.getElementById('demoDurationSlider').value;
    const durationMs  = parseInt(durationSec) * 1000;
    fetch(`/api/demomode?active=1&duration=${durationMs}`)
        .then(r => r.json())
        .then(j => { /* UI update via polling status */ });
}

function stopDemo() {
    fetch('/api/demomode?active=0').then(r => r.json());
}

// In de bestaande status-poll loop:
function applyStatus(s) {
    // ... bestaande velden ...
    const demoBtn = document.getElementById('btnDemo');
    if (s.demoActive) {
        demoBtn.classList.add('demo-running');
        demoBtn.textContent = `Demo (${s.demoChapter+1}/7)`;
    } else {
        demoBtn.classList.remove('demo-running');
        demoBtn.textContent = 'Demo';
    }
}
```

---

## 10. `sdroot/index.html`  (knop + slider)

```html
<!-- Bij de andere mode-knoppen (TV, etc.) -->
<div class="demo-control">
    <button id="btnDemo" class="demo-btn" onclick="startDemo()">Demo</button>
    <input type="range" id="demoDurationSlider"
           min="30" max="180" value="90" step="10"
           class="slider-yellow">
    <span id="demoDurationLabel">90s</span>
</div>
```

---

## 11. `sdroot/styles.css`

```css
.demo-btn {
    background: #FFC107;        /* geel */
    /* ... */
}
.demo-btn.demo-running {
    background: #FF9800;        /* oranje pulserend */
    animation: pulse 1s infinite;
}
.slider-yellow::-webkit-slider-thumb {
    background: #FFC107;
}
```

---

## 12. `sdroot/light_patterns.csv`  (data, jij vult parameters)

Nieuwe rijen voor IDs 50–56:

| id | naam       | bright_cycle_sec | color_cycle_sec | fadeWidth | radius | radiusOsc | x_amp | y_amp | gradientSpeed | windowWidth | pnf |
|----|------------|-------------------|------------------|-----------|--------|-----------|-------|-------|----------------|-------------|-----|
| 50 | DEMO_OPEN  | (te bepalen) — Color Bath stijl, hele Kwal langzame puls |
| 51 | DEMO_LAYER | Layered Glow — ringen op eigen ritme |
| 52 | DEMO_PRIDE | Pride/Plasma — vloeiend, rijk |
| 53 | DEMO_PIN   | Pinwheel — pseudo-rotatie via x_amp/y_amp |
| 54 | DEMO_CONF  | Confetti-achtig — willekeurig sprankelend |
| 55 | DEMO_DISCO | Disco — snelle bright-puls (sin → flits-achtig) |
| 56 | DEMO_HEART | Heartbeat — dubbele puls per cyclus |

---

## 13. `sdroot/light_colors.csv`  (data, jij vult parameters)

Nieuwe rijen voor IDs 50–56 (kleurparen passend bij elk chapter).

---

## 14. `/150/` op SD-kaart  (audio, jij maakt MP3's)

```
/150/001.mp3   TTS opening "Dit is Kwal..."
/150/002.mp3   Bel/sirene (~2 sec)
/150/003.mp3   TTS "Tientallen patronen..."
/150/004.mp3   Muziek/sfx voor layered glow chapter
/150/005.mp3   TTS "Eindeloze kleurcombinaties..."
/150/006.mp3   Muziek voor pride/plasma
/150/007.mp3   TTS "Soms feestelijk..."
/150/008.mp3   Kermismuziek
/150/009.mp3   TTS "...uitbundig"
/150/010.mp3   Feestmuziek
/150/011.mp3   TTS (kort, voor disco-intro)
/150/012.mp3   Disco-track met vaste BPM
/150/013.mp3   TTS afsluiting "Elke dag anders..."
/150/014.mp3   Hartslag-geluid + fade
```

---

## SAMENVATTING — wat moet aangepast/aangemaakt

Firmware (C++):
- [x] `lib/Globals/Globals.h`            — al: demoDir, demoActive, demoTotalMs, demoChapterIdx, demoPhase
- [x] `lib/Globals/Globals.cpp`          — al: demoDir CSV parser
- [ ] `lib/RunManager/Demo/DemoRun.h`    — nieuw
- [ ] `lib/RunManager/Demo/DemoRun.cpp`  — nieuw (chapter-tabel, callbacks)
- [ ] `lib/RunManager/RunManager.cpp`    — `if (demoActive) return;` guards
- [ ] `lib/WebInterfaceController/Handlers/DemoMode.cpp` — nieuw (web handler)
- [ ] `lib/WebInterfaceController/Routes.cpp` (of equiv.) — route registratie
- [ ] `lib/WebInterfaceController/WebGuiStatus.cpp`     — status JSON velden

WebGUI:
- [ ] `sdroot/index.html`                — knop + gele slider
- [ ] `sdroot/styles.css`                — `.demo-btn`, `.slider-yellow`
- [ ] `sdroot/webgui-src/js/controls.js` — `startDemo()`, `stopDemo()`, status

Data (gebruiker):
- [ ] `sdroot/light_patterns.csv`        — 7 nieuwe rijen (IDs 50–56)
- [ ] `sdroot/light_colors.csv`          — 7 nieuwe rijen (IDs 50–56)
- [ ] `/150/001.mp3` … `/150/014.mp3`    — door gebruiker aangeleverd

Build/deploy:
- [ ] `sdroot/webgui-src/build.ps1`      — JS version bump als JS edited
- [ ] Firmware version bump in `lib/Globals/Globals.h`

---

## OPEN PUNTEN (niet in pseudo, te beslissen)

1. **Hoe wordt MP3-duur bekend?** `getMp3DurationMs()` bestaat al
   (bewijs nodig — anders `frag.durationMs = 0` + callback bij audio-end).
2. **Audio-feedback hook voor color-morph chapter 1**: welke globale
   variabele expose de huidige amplitude/beat?
3. **Slider-schaling**: chapter-tabel is nu vast. Bij korte slider-duur
   (30s) → eerste N chapters die binnen 30s passen? Of alle chapters
   met proportioneel verkorte showMs?
4. **Pattern/colors restore na demo**: bewaar pre-demo state en herstel
   bij `stop()`?

---

## AFHANKELIJKHEDEN (externe features)

### CalendarSelector::getNextEvent()  (zie `docs/calendar_pseudo.md`)

Demo-chapter 22 (kalender) gebruikt deze nieuwe API om bij gewone dagen
te kunnen vertellen welk event er aankomt — zonder live SD-scan.

Verwacht gedrag in demo-prep (achtergrond TTS-render):
```
if calendarSelector.getNextEvent(info):
    text = "de eerstvolgende bijzondere dag is " + info.ttsSentence
         + ", over " + info.daysFromToday + " dagen"
else:
    text = "vandaag is een gewone dag in kwal's kalender"

downloadTtsToCache(text, dirIndex=150, fileIndex=86)
```

Bij event-dag vandaag: gewone `loadToday()` levert al een `CalendarEntry`
op — demo gebruikt die ttsSentence direct, geen "next event" nodig.

**Status:** deze afhankelijkheid wordt apart geïmplementeerd
(eigen pseudo + eigen versie-bump). Demo wacht hier niet op:
zonder de cache returnt `getNextEvent()` false en valt demo terug
op "vandaag is een gewone dag".
