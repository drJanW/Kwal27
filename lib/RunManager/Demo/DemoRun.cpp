/**
 * @file DemoRun.cpp
 * @brief 31-chapter demo orchestrator
 * @version 260607F
 * @date 2026-06-07
 * this demo is a fuck up by copilot-new-style - it costed >$120 to create this mess
 * Per chapter (3-step state machine):
 *   Step 0: lux-check + set pattern/colors + start TTS, schedule lightChange
 *           at LIGHT_DELAY_BASE_MS after TTS start.
 *   cb_demoLightChange: apply pattern + colors (with optional shifts).
 *   cb_demoStep1: start audio, scheduled at (estTtsMs + AUDIO_DELAY_BASE_MS).
 *   cb_demoStep2: advance to next chapter.
 *
 * Ch1: own flash renderer (PlayLightShow CRGB::White/Black, restart() for varying interval).
 * Ch17-21: live-TTS (086-090.mp3 pre-rendered at start() via TtsTodoQueue).
 * Ch29: CF_TV_START — enter tvMode(4h) and stop demo.
 * Ch22-28: CF_SHIFT_PATTERN / CF_SHIFT_COLORS — setDemoBit() for shift CSV rows.
 */
#include <Arduino.h>
#include <SD.h>

#include "DemoRun.h"
#include "Globals.h"
#include "TimerManager.h"
#include "PlayFragment.h"
#include "Light/LightRun.h"
#include "SdFileAccess.h"
#include "SensorController.h"
#include "ContextController.h"
#include "Calendar.h"
#include "StatusFlags.h"
#include "StatusBits.h"
#include "Alert/AlertState.h"
#include "RunManager.h"
#include "AudioState.h"
#include "HWconfig.h"
#include "Tts/TtsTodoQueue.h"
#include "LightController.h"
#include "RingRenderer.h"
#include "WebGuiStatus.h"
#include "PRTClock.h"
#include <FastLED.h>

namespace {

// ─── Chapter table ────────────────────────────────────────────────────────

enum ChapterFlag : uint8_t {
    CF_NONE          = 0,
    CF_SHIFT_PATTERN = 1 << 0,
    CF_SHIFT_COLORS  = 1 << 1,
    CF_TV_START      = 1 << 2,
    CF_LIVE_TTS      = 1 << 3,
    CF_DYN_COLOR     = 1 << 4,
    CF_DYN_PATTERN   = 1 << 5,
    CF_RING_SCENE    = 1 << 6,  // direct ring renderer, no pattern/colors CSV
};

struct Chapter {
    uint8_t  ttsFile;
    uint8_t  audioFile;
    uint16_t audioMaxMs;
    uint8_t  patternId;
    uint8_t  colorId;
    uint8_t  flags;
    uint8_t  maxLux;
};

static const Chapter chapters[] = {
    // tts  aud   audMs  pat  col  flags                          maxLux
    {  1,  51,  5000,    0,   0, CF_NONE,                            253},  // 01 witte flitsen
    {  2,  52,  5000,   26,  30, CF_NONE,                            253},  // 02 welkom (Emerald Isle)
    {  3,  53,  5000,   27,   2, CF_NONE,                            253},  // 03 blauwe ringen
//    {  4,  54,  5000,   27,  40, CF_NONE,                            253},  // 04 witte ringen
//    {  5,  55,  5000,    9,  34, CF_NONE,                            253},  // 05 blauw-in-wit (Gentle Waves)
    {  6,  56,  8000,    0,   0, CF_RING_SCENE,                      253},  // 06 extreem zappa
    {  8,  58,  8000,    3,  38, CF_NONE,                            253},  // 08 disco (Magenta Dream)
    {  9,  59, 30000,   32,  32, CF_NONE,                            253},  // 09 spectrum
    { 11,  61,  8000,    7,  41, CF_NONE,                            253},  // 11 lounge (Radiant Glow)
    { 13,  63,  8000,   27,  25, CF_NONE,                            253},  // 13 noorderlicht
    { 15,  65,  8000,   28,   8, CF_NONE,                            253},  // 15 sterren (Sky Blue)
    { 16,  66,  8000,    5,  38, CF_NONE,                            253},  // 16 rust (Plum Purple)
    { 17,  86,  3000,    5,  18, CF_LIVE_TTS,                          0},  // 17 tijd (Golden Glow)
    { 18,  87,  3000,    7,   0, CF_LIVE_TTS|CF_DYN_COLOR,             0},  // 18 kamer-temp
    { 19,  88,  4000,    1,   0, CF_LIVE_TTS|CF_DYN_COLOR,             0},  // 19 zon
    { 20,  89,  3000,    5,  16, CF_LIVE_TTS,                        253},  // 20 maan (Lavender Mist)
    { 29,  79,  5000,    0,   0, CF_TV_START,                        253},  // 22 TV-sim: 20s
    { 30,  80,  6000,   28,   5, CF_NONE,                            253},  // 23 vuurwerk
    { 31,  81,  8000,    2,  10, CF_NONE,                            253},  // 24 afscheid
};
static constexpr uint8_t CHAPTER_COUNT = sizeof(chapters) / sizeof(chapters[0]);

constexpr uint16_t LIGHT_DELAY_BASE_MS = 1000;
constexpr uint16_t AUDIO_DELAY_BASE_MS =  800;  // fixed gap TTS→audio, not scaled
constexpr uint16_t MIN_STEP_MS         = 100;

constexpr uint32_t NATURAL_TOTAL_MS = 290000UL;  // 24-chapter table (was 360000 with 31 chapters)

static float   audioFactor_    = 1.0f;
static bool    demoRingActive_ = false;
static uint8_t ringHue_        = 0;

// Pair-drop table (last pair first)
static const uint8_t dropPairs[][2] = {
    {31, 30}, {29, 28}, {27, 21}, {26, 20}, {25, 19},
    {24, 18}, {23, 17}, {22, 16},
};
static constexpr uint8_t DROP_PAIR_COUNT = sizeof(dropPairs) / sizeof(dropPairs[0]);
static bool skipChapter_[CHAPTER_COUNT] = {};

// ─── Flash renderer (ch1) ─────────────────────────────────────────────────

static uint8_t flashRemaining_ = 0;
static bool    flashOn_        = false;

void cb_demoFlash();

void cb_demoFlash() {
    if (flashRemaining_ == 0) return;
    flashOn_ = !flashOn_;
    if (flashOn_) {
        PlayLightShow(MakeSolidParams(CRGB::White));
        timers.restart(100, 1, cb_demoFlash);
    } else {
        PlayLightShow(MakeSolidParams(CRGB::Black));
        flashRemaining_--;
        if (flashRemaining_ > 0) {
            timers.restart(900, 1, cb_demoFlash);
        }
    }
}

void startFlashSequence(uint8_t cycles) {
    flashRemaining_ = cycles;
    flashOn_        = false;
    cb_demoFlash();
}

// ─── Ring scene renderer (ch06, ch07) ───────────────────────────────────────

void cb_demoRingCh06();
void cb_demoThunder();

void cb_demoRingCh06() {
    if (!Globals::demoActive || !demoRingActive_) return;
    ringHue_ += 20;
    RingTarget targets[RING_COUNT];
    for (int z = 0; z < RING_COUNT; z++) {
        targets[z].color      = CHSV(ringHue_ + z * 42, 255, 240);
        targets[z].brightness = 220;
        targets[z].instant    = true;
    }
    setRingTargets(targets);
}

void cb_demoThunder() {
    if (!Globals::demoActive || !demoRingActive_) return;
    ringHue_ += 42;
    RingTarget targets[RING_COUNT];
    for (int z = 0; z < RING_COUNT; z++) {
        targets[z].color      = CHSV(ringHue_ + z * 43, 255, 255);
        targets[z].brightness = 255;
        targets[z].instant    = true;
    }
    setRingTargets(targets);
}

// ─── Helpers ──────────────────────────────────────────────────────────────

uint16_t scaledAudioMs(uint16_t raw) {
    if (raw == 0) return 0;
    float v = raw * audioFactor_;
    if (v < 200.0f)   v = 200.0f;
    if (v > 60000.0f) v = 60000.0f;
    return static_cast<uint16_t>(v);
}

uint32_t estimateTtsMs(uint8_t dir, uint8_t file) {
    if (AlertState::isSdBusy()) return 0;
    const char* path = getMP3Path(dir, file);
    File f = SD.open(path, FILE_READ);
    if (!f) return 0;
    size_t bytes = f.size();
    f.close();
    if (bytes == 0) return 0;
    return static_cast<uint32_t>((bytes * Globals::ttsCacheDurationFactor) / 16);
}

uint8_t selectColorForTemp(float tempC) {
    if (tempC < 18.0f) return 2;   // Cool Ocean
    if (tempC < 23.0f) return 5;   // Sunny Yellow
    return 14;                      // Citrus Orange
}

uint8_t selectColorForDaypart(const ContextController::TimeState& t) {
    int nowMins     = prtClock.getHour() * 60 + prtClock.getMinute();
    int sunriseMins = prtClock.getSunriseHour() * 60 + prtClock.getSunriseMinute();
    int sunsetMins  = prtClock.getSunsetHour()  * 60 + prtClock.getSunsetMinute();
    if (nowMins < sunriseMins)         return 11;  // Midnight Blue
    if (nowMins < sunriseMins + 90)    return 31;  // Sunrise Pink
    if (nowMins < sunsetMins  - 60)    return  5;  // Sunny Yellow
    if (nowMins < sunsetMins  + 30)    return 14;  // Citrus Orange
    return 31;  // Sunrise Pink (post-sunset = roze avondlucht)
}

const char* moonPhaseWord(float phase) {
    if (phase < 0.05f || phase > 0.95f) return "nieuwe";
    if (phase < 0.20f)                  return "wassende halve";
    if (phase < 0.30f)                  return "halve";
    if (phase < 0.45f)                  return "wassende";
    if (phase < 0.55f)                  return "volle";
    if (phase < 0.70f)                  return "afnemende";
    if (phase < 0.80f)                  return "halve afnemende";
    return "afnemende sikkel";
}

void appendCsvLine(const char* line) {
    if (AlertState::isSdBusy()) return;
    File f = SD.open("/tts_todo.csv", FILE_APPEND);
    if (!f) return;
    f.println(line);
    f.close();
}

void appendLiveTtsLines() {
    const auto& t = ContextController::time();
    char buf[200];

    // 17 tijd
    snprintf(buf, sizeof(buf),
        "Bram; -1; 99; 150; 86; het is nu %u uur %u",
        prtClock.getHour(), prtClock.getMinute());
    appendCsvLine(buf);

    // 18 buiten-temp (via weather fetch)
    if (t.hasWeather) {
        float avg = (t.weatherMinC + t.weatherMaxC) / 2.0f;
        snprintf(buf, sizeof(buf),
            "Bram; -1; 99; 150; 87; het is buiten %.0f graden",
            avg);
    } else {
        snprintf(buf, sizeof(buf),
            "Bram; -1; 99; 150; 87; de buitentemperatuur kan ik nu niet meten");
    }
    appendCsvLine(buf);

    // 19 zon
    snprintf(buf, sizeof(buf),
        "Bram; -1; 99; 150; 88; de zon kwam vanmorgen om %u uur %u op en gaat onder om %u uur %u",
        prtClock.getSunriseHour(), prtClock.getSunriseMinute(), prtClock.getSunsetHour(), prtClock.getSunsetMinute());
    appendCsvLine(buf);

    // 20 maan
    snprintf(buf, sizeof(buf),
        "Bram; -1; 99; 150; 89; vanavond zien we een %s maan",
        moonPhaseWord(prtClock.getMoonPhaseValue()));
    appendCsvLine(buf);

    // 21 kalender — gebruik gecachede next event van calendarSelector
    const CalendarData& cal = calendarSelector.calendarData();
    bool hasToday = cal.valid && !cal.day.ttsSentence.isEmpty();
    NextEventInfo nxt{};
    bool hasNext = calendarSelector.getNextEvent(nxt);
    if (hasToday && hasNext && nxt.daysFromToday > 0) {
        snprintf(buf, sizeof(buf),
            "Bram; -1; 99; 150; 90; vandaag is %s - de eerstvolgende bijzondere dag is over %u dagen",
            cal.day.ttsSentence.c_str(), nxt.daysFromToday);
    } else if (hasToday) {
        snprintf(buf, sizeof(buf),
            "Bram; -1; 99; 150; 90; vandaag is %s",
            cal.day.ttsSentence.c_str());
    } else if (hasNext && nxt.daysFromToday > 0) {
        snprintf(buf, sizeof(buf),
            "Bram; -1; 99; 150; 90; vandaag is een gewone dag - de eerstvolgende bijzondere dag is over %u dagen",
            nxt.daysFromToday);
    } else {
        snprintf(buf, sizeof(buf),
            "Bram; -1; 99; 150; 90; vandaag is een gewone dag in kwal's kalender");
    }
    appendCsvLine(buf);
}

// ─── State machine ────────────────────────────────────────────────────────

void cb_demoRingCh06();
void cb_demoThunder();
void cb_demoInit();
void cb_demoLightChange();
void cb_demoStep1();
void cb_demoStep2();
void startStep0();

static uint32_t currentTtsMs_ = 0;

void cb_demoLightChange() {
    if (!Globals::demoActive) return;
    const Chapter& ch = chapters[Globals::demoChapterIdx];

    // Ring scene chapters: activate direct ring renderer
    if (ch.flags & CF_RING_SCENE) {
        Globals::tvMode    = true;
        FastLED.setBrightness(Globals::tvMaxBrightness);
        demoRingActive_    = true;
        ringHue_           = 0;
        if (ch.ttsFile == 6) {  // ch06 zappa
            timers.create(120, 0, cb_demoRingCh06);
        } else if (ch.ttsFile == 7) {  // ch07 kermis
            timers.create(80, 0, cb_demoThunder);
        }
        return;
    }

    // Set demo shift bits
    StatusFlags::setDemoBit(STATUS_DEMO_PAT_SHIFT, ch.flags & CF_SHIFT_PATTERN);
    StatusFlags::setDemoBit(STATUS_DEMO_COL_SHIFT,  ch.flags & CF_SHIFT_COLORS);

    // Resolve dynamic color/pattern
    uint8_t pat = ch.patternId;
    uint8_t col = ch.colorId;

    if (ch.flags & CF_DYN_COLOR) {
        const auto& t = ContextController::time();
        if (ch.ttsFile == 18) {  // ch18: kamer-temp
            float avg = t.hasWeather ? (t.weatherMinC + t.weatherMaxC) / 2.0f : 15.0f;
            col = selectColorForTemp(avg);
        } else if (ch.ttsFile == 19) {  // ch19: zon
            col = selectColorForDaypart(t);
        } else if (ch.ttsFile == 21) {  // ch21: kalender (today → today's colors, else next event's colors)
            const CalendarData& cal = calendarSelector.calendarData();
            if (cal.valid && cal.day.valid && cal.day.colorId != 0) {
                col = cal.day.colorId;
                PF("[Demo] ch21 col=today:%u\n", col);
            } else {
                NextEventInfo nxt{};
                if (calendarSelector.getNextEvent(nxt) && nxt.colorId != 0) {
                    col = nxt.colorId;
                    PF("[Demo] ch21 col=next:%u\n", col);
                } else {
                    col = 5;   // fallback Sunny Yellow
                    PF("[Demo] ch21 col=fallback:21\n");
                }
            }
        }
    }
    if (ch.flags & CF_DYN_PATTERN) {
        const CalendarData& cal = calendarSelector.calendarData();
        if (cal.valid && cal.day.valid && cal.day.patternId != 0) {
            pat = cal.day.patternId;
        } else {
            NextEventInfo nxt{};
            if (calendarSelector.getNextEvent(nxt) && nxt.patternId != 0)
                pat = nxt.patternId;
            else
                pat = 2;  // fallback Slow Breathing
        }
    }

    if (pat != 0) LightRun::applyPattern(pat);
    if (col != 0) LightRun::applyColor(col);
}

void cb_demoStep1() {
    if (!Globals::demoActive) return;
    if (Globals::demoChapterIdx >= CHAPTER_COUNT) { DemoRun::stop(); return; }
    const Chapter& ch = chapters[Globals::demoChapterIdx];

    // Ch1: flash renderer instead of audio
    if (ch.ttsFile == 1) {
        startFlashSequence(5);
        uint32_t waitMs = scaledAudioMs(ch.audioMaxMs);
        if (waitMs < MIN_STEP_MS) waitMs = MIN_STEP_MS;
        timers.create(waitMs, 1, cb_demoStep2);
        return;
    }

    if (ch.audioFile == 0) {
        // ch29 (CF_TV_START) schedules cb_demoStep2 itself via 20s timer — don't double-schedule
        if (!(ch.flags & CF_TV_START)) {
            timers.create(MIN_STEP_MS, 1, cb_demoStep2);
        }
        return;
    }

    // For CF_LIVE_TTS chapters: check if file exists
    if (ch.flags & CF_LIVE_TTS) {
        const char* path = getMP3Path(Globals::demoDir, ch.audioFile);
        if (!SD.exists(path)) {
            PF("[Demo] live-TTS %u/%u missing, skip ch%u\n",
               Globals::demoDir, ch.audioFile,
               Globals::demoChapterIdx + 1);
            Globals::demoChapterIdx++;
            startStep0();
            return;
        }
    }

    uint16_t playMs = scaledAudioMs(ch.audioMaxMs);
    AudioFragment frag = {};
    frag.dirIndex   = Globals::demoDir;
    frag.fileIndex  = ch.audioFile;
    frag.startMs    = 0;
    frag.durationMs = playMs;
    frag.fadeMs     = 200;
    strncpy(frag.source, "demo-audio", sizeof(frag.source) - 1);
    PlayAudioFragment::start(frag);

    timers.create(playMs, 1, cb_demoStep2);
}

void cb_demoStep2() {
    if (!Globals::demoActive) return;
    timers.cancel(cb_demoRingCh06);
    timers.cancel(cb_demoFlash); timers.cancel(cb_demoThunder);
    if (demoRingActive_) {
        Globals::tvMode = false;
        demoRingActive_ = false;
    } else if (Globals::tvMode) {
        RunManager::exitTvMode();
    }
    Globals::demoChapterIdx++;
    startStep0();
}

void startStep0() {
    if (!Globals::demoActive) return;

    // Skip finished or marked-skip chapters
    while (Globals::demoChapterIdx < CHAPTER_COUNT) {
        if (skipChapter_[Globals::demoChapterIdx]) {
            Globals::demoChapterIdx++;
            continue;
        }
        const Chapter& ch = chapters[Globals::demoChapterIdx];
        if (ch.ttsFile == 0 && ch.audioFile == 0) {
            // Placeholder (ch10): skip silently
            Globals::demoChapterIdx++;
            continue;
        }
        break;
    }

    if (Globals::demoChapterIdx >= CHAPTER_COUNT) {
        DemoRun::stop();
        return;
    }

    const Chapter& ch = chapters[Globals::demoChapterIdx];

    // TV-mode trigger (ch29): 20s demo-preview dan terug naar normale demo
    if (ch.flags & CF_TV_START) {
        PL("[Demo] tvMode preview (20s)");
        RunManager::enterTvMode(1);
        timers.create(SECONDS(20), 1, cb_demoStep2, 1.0f, 2);  // token=2: anders dan step-advance token
        return;
    }

    // Lux check
    if (ch.maxLux > 0) {
        float lux = SensorController::ambientLux();
        if (lux > static_cast<float>(ch.maxLux)) {
            PF("[Demo] skip ch%u (lux %.0f > %u)\n",
               Globals::demoChapterIdx + 1, lux, ch.maxLux);
            Globals::demoChapterIdx++;
            startStep0();
            return;
        }
    }

    PF("[Demo] ch%u/%u tts=%u audio=%u pat=%u col=%u flags=%u\n",
       Globals::demoChapterIdx + 1, CHAPTER_COUNT,
       ch.ttsFile, ch.audioFile, ch.patternId, ch.colorId, ch.flags);

    // Start TTS
    currentTtsMs_ = 0;
    if (ch.ttsFile != 0) {
        currentTtsMs_ = estimateTtsMs(Globals::demoDir, ch.ttsFile);
        if (currentTtsMs_ > 0) {
            AudioFragment frag = {};
            frag.dirIndex   = Globals::demoDir;
            frag.fileIndex  = ch.ttsFile;
            frag.startMs    = 0;
            frag.durationMs = currentTtsMs_ + 1000;
            frag.fadeMs     = 0;
            strncpy(frag.source, "demo-tts", sizeof(frag.source) - 1);
            PlayAudioFragment::start(frag);
        }
    }

    // Schedule light change at LIGHT_DELAY_BASE_MS after TTS start
    uint32_t lightDelayMs = LIGHT_DELAY_BASE_MS;
    if (lightDelayMs < MIN_STEP_MS) lightDelayMs = MIN_STEP_MS;
    timers.create(lightDelayMs, 1, cb_demoLightChange);

    // Schedule audio at tts_done + fixed AUDIO_DELAY_BASE_MS (not scaled)
    uint32_t audioDelayMs = currentTtsMs_ + AUDIO_DELAY_BASE_MS;
    if (audioDelayMs < MIN_STEP_MS) audioDelayMs = MIN_STEP_MS;
    timers.create(audioDelayMs, 1, cb_demoStep1);
}

void cb_demoInit() {
    if (!Globals::demoActive) return;
    RunManager::requestSetDemoVolume();            // max volume, no web expiry
    setBrightnessShiftedHi(Globals::brightnessHi); // force max brightness directly
    appendLiveTtsLines();
    TtsTodoQueue::startNow();
    startStep0();
}

}  // anonymous namespace

namespace DemoRun {

void start() {
    if (Globals::demoActive) return;
    Globals::demoActive     = true;
    Globals::demoChapterIdx = 0;

    // Clear skip table
    memset(skipChapter_, 0, sizeof(skipChapter_));

    // Compute audio scale factor
    uint32_t total = Globals::demoTotalMs;
    if (total < 30000UL)  total = 30000UL;
    if (total > 900000UL) total = 900000UL;
    audioFactor_ = static_cast<float>(total) / static_cast<float>(NATURAL_TOTAL_MS);
    if (audioFactor_ < 0.10f) audioFactor_ = 0.10f;
    if (audioFactor_ > 3.00f) audioFactor_ = 3.00f;

    // Pair-drop for short demos
    // Drop pairs while factor <= 0.75 (slider < 75% of natural duration)
    uint8_t dropIdx = 0;
    while (audioFactor_ <= 0.75f && dropIdx < DROP_PAIR_COUNT) {
        uint8_t a = dropPairs[dropIdx][0] - 1;  // 0-based
        uint8_t b = dropPairs[dropIdx][1] - 1;
        if (a < CHAPTER_COUNT) skipChapter_[a] = true;
        if (b < CHAPTER_COUNT) skipChapter_[b] = true;
        dropIdx++;
        // Recompute factor with fewer chapters (rough: subtract 8000ms per dropped pair)
        total += 8000;
        audioFactor_ = static_cast<float>(Globals::demoTotalMs) /
                       static_cast<float>(NATURAL_TOTAL_MS - dropIdx * 8000UL);
    }

    PF("[Demo] start (%u chapters, total=%lums, factor=%.2f)\n",
       CHAPTER_COUNT,
       static_cast<unsigned long>(Globals::demoTotalMs),
       static_cast<double>(audioFactor_));
    // Defer SD I/O to timer callback (web handler must not touch SD)
    timers.create(1, 1, cb_demoInit);
}

void stop() {
    Globals::demoActive = false;
    timers.cancel(cb_demoLightChange);
    timers.cancel(cb_demoStep1);
    timers.cancel(cb_demoStep2);        // token=1 (chapter advance)
    timers.cancel(cb_demoStep2, 2);     // token=2 (TV-sim 20s)
    timers.cancel(cb_demoFlash);
    timers.cancel(cb_demoRingCh06);
    timers.cancel(cb_demoFlash); timers.cancel(cb_demoThunder);
    StatusFlags::setDemoBit(STATUS_DEMO_PAT_SHIFT, false);
    StatusFlags::setDemoBit(STATUS_DEMO_COL_SHIFT,  false);
    if (demoRingActive_) {
        Globals::tvMode = false;
        demoRingActive_ = false;
    } else if (Globals::tvMode) {
        RunManager::exitTvMode();
    }
    PlayAudioFragment::stop(500);
    WebGuiStatus::pushState();
    PL("[Demo] stop — reboot in 5s");
    timers.create(SECONDS(5), 1, []() {
        Serial.flush();
        ESP.restart();
    });
}

bool isActive() { return Globals::demoActive; }
uint8_t chapterCount() { return CHAPTER_COUNT; }

}  // namespace DemoRun



