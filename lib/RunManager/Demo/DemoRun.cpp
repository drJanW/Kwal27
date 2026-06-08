/**
 * @file DemoRun.cpp
 * @brief 17-chapter demo orchestrator
 * @version 260608H
 * @date 2026-06-08
 * this demo is a fuck up by copilot-new-style - it costed >$120 to create this mess
 *
 * Timer chain per chapter (no state machine, no recursion):
 *   cb_demoChapterStart  — idx advance, TTS start
 *     └─ create(LIGHT_DELAY, 1, cb_demoChapterLight)
 *   cb_demoChapterLight  — apply pattern/colors/shifts, or start ring/flash
 *     └─ create(audioDelay - LIGHT_DELAY, 1, cb_demoChapterAudio)
 *   cb_demoChapterAudio  — start audio fragment
 *     └─ create(audioMs, 1, cb_demoChapterDone)
 *   cb_demoChapterDone   — cleanup ring/flash, idx++
 *     └─ create(1, 1, cb_demoChapterStart)   (1ms breaks call stack)
 *
 * Exceptions:
 *   CF_TV_START  → cb_demoChapterStart calls enterTvMode + stop, chain ends.
 *   CF_RING_SCENE → cb_demoChapterLight starts infinite ring timer;
 *                   cb_demoChapterDone cancels it.
 *   CF_LIVE_TTS  → cb_demoChapterAudio skips if file missing.
 *   Ch1 flash    → cb_demoChapterLight starts cb_demoFlash chain.
 *
 * Ch11-14: live-TTS (086-089.mp3 pre-rendered at start() via TtsTodoQueue).
 */
#include <Arduino.h>
#include <SD.h>

#include "DemoRun.h"
#include "Globals.h"
#include "TimerManager.h"
#include "PlayFragment.h"
#include "Light/LightRun.h"
#include "SdFileAccess.h"
#include "ContextController.h"
#include "StatusFlags.h"
#include "StatusBits.h"
#include "Alert/AlertState.h"
#include "Alert/AlertRGB.h"
#include "RunManager.h"
#include "Tts/TtsTodoQueue.h"
#include "LightController.h"
#include "RingRenderer.h"
#include "WebGuiStatus.h"
#include "PRTClock.h"
#include <FastLED.h>
#include "MathUtils.h"

namespace {

// ─── Chapter table ────────────────────────────────────────────────────────

enum ChapterFlag : uint8_t {
    CF_NONE          = 0,
    CF_SHIFT_PATTERN = 1 << 0,
    CF_SHIFT_COLORS  = 1 << 1,
    CF_TV_START      = 1 << 2,
    CF_LIVE_TTS      = 1 << 3,
    CF_DYN_COLOR     = 1 << 4,
    CF_RING_SCENE    = 1 << 6,  // direct ring renderer, no pattern/colors CSV
};

struct Chapter {
    uint8_t  ttsFile;
    uint8_t  audioFile;
    uint16_t audioMaxMs;
    uint8_t  patternId;
    uint8_t  colorId;
    uint8_t  flags;
};

static const Chapter chapters[] = {
    // tts  aud   audMs  pat  col  flags
    {  1,  51,  5000,    0,   0, CF_NONE          },  // 01 witte flitsen
    {  2,  52,  5000,   26,  30, CF_NONE          },  // 02 welkom (Emerald Isle)
    {  3,  53,  5000,   27,   2, CF_NONE          },  // 03 blauwe ringen
    {  6,  56,  8000,    0,   0, CF_RING_SCENE    },  // 06 extreem zappa
    {  8,  58,  8000,    3,  38, CF_NONE          },  // 08 disco (Magenta Dream)
    {  9,  59, 30000,   32,  32, CF_NONE          },  // 09 spectrum
    { 11,  61,  8000,    7,  41, CF_NONE          },  // 11 lounge (Radiant Glow)
    { 13,  63,  8000,   27,  25, CF_NONE          },  // 13 noorderlicht
    { 15,  65,  8000,   28,   8, CF_NONE          },  // 15 sterren (Sky Blue)
    { 17,  86,  3000,    5,  18, CF_LIVE_TTS      },  // 17 tijd (Golden Glow)
    { 18,  87,  3000,    7,   0, CF_LIVE_TTS|CF_DYN_COLOR },  // 18 kamer-temp
    { 19,  88,  4000,    1,   0, CF_LIVE_TTS|CF_DYN_COLOR },  // 19 zon
    { 20,  89,  3000,    5,  16, CF_LIVE_TTS      },  // 20 maan (Lavender Mist)
    { 29,  79,  5000,    0,   0, CF_TV_START      },  // TV-sim: 20s
    { 30,  80,  6000,   28,   5, CF_NONE          },  // vuurwerk
    { 31,  81,  8000,    2,  10, CF_NONE          },  // afscheid
};
static constexpr uint8_t CHAPTER_COUNT = sizeof(chapters) / sizeof(chapters[0]);

constexpr uint16_t LIGHT_DELAY_BASE_MS = 1000;
constexpr uint16_t AUDIO_DELAY_BASE_MS =   10;  // fixed gap after TTS fragment busy clears
constexpr uint16_t MIN_STEP_MS         = 100;

constexpr uint32_t NATURAL_TOTAL_MS = 290000UL;  // 17-chapter table

static float   audioFactor_    = 1.0f;
static bool    demoRingActive_ = false;
static uint8_t ringHue_        = 0;

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
    timers.create(1, 1, cb_demoFlash);
}

// ─── Ring scene renderer (ch06) ─────────────────────────────────────────────

void cb_demoRingCh06();

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

// ─── Helpers ──────────────────────────────────────────────────────────────

uint16_t scaledAudioMs(uint16_t raw) {
    if (raw == 0) return 0;
    float v = MathUtils::clamp(raw * audioFactor_, 200.0f, 60000.0f);
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

uint8_t selectColorForDaypart() {
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
        "NL; 1; -1; 150; 86; het is nu %u uur %u",
        prtClock.getHour(), prtClock.getMinute());
    appendCsvLine(buf);

    // 18 buiten-temp (via weather fetch)
    if (t.hasWeather) {
        float avg = (t.weatherMinC + t.weatherMaxC) / 2.0f;
        snprintf(buf, sizeof(buf),
            "NL; 1; -1; 150; 87; het is buiten %.0f graden",
            avg);
    } else {
        snprintf(buf, sizeof(buf),
            "NL; 1; -1; 150; 87; de buitentemperatuur kan ik nu niet meten");
    }
    appendCsvLine(buf);

    // 19 zon — skip if sunrise not yet fetched
    if (prtClock.getSunriseHour() == 0 && prtClock.getSunriseMinute() == 0) {
        PL("[Demo] ch19 skip: sunrise not yet fetched");
    } else {
        snprintf(buf, sizeof(buf),
            "NL; 1; -1; 150; 88; de zon kwam vanmorgen om %u uur %u op en gaat onder om %u uur %u",
            prtClock.getSunriseHour(), prtClock.getSunriseMinute(),
            prtClock.getSunsetHour(), prtClock.getSunsetMinute());
        appendCsvLine(buf);
    }

    // 20 maan
    snprintf(buf, sizeof(buf),
        "NL; 1; -1; 150; 89; vanavond zien we een %s maan",
        moonPhaseWord(prtClock.getMoonPhaseValue()));
    appendCsvLine(buf);
}

// ─── Timer chain ─────────────────────────────────────────────────────────

void cb_demoChapterStart();
void cb_demoChapterLight();
void cb_demoChapterAudio();
void cb_demoChapterDone();
void cb_demoTvEnter();
void cb_demoInitLiveTts();

static uint32_t currentTtsMs_   = 0;

void cb_demoTvEnter() {
    if (!Globals::demoActive) return;
    PL("[Demo] tvMode preview (20s)");
    RunManager::enterTvMode(1);
    timers.create(SECONDS(20), 1, cb_demoChapterDone);
}

void cb_demoInitLiveTts() {
    if (!Globals::demoActive) return;
    appendLiveTtsLines();
    TtsTodoQueue::startNow();
}

// Apply pattern + colors for current chapter (shared by light callback)
void applyChapterLight() {
    const Chapter& ch = chapters[Globals::demoChapterIdx];

    if (ch.flags & CF_RING_SCENE) {
        Globals::tvMode = true;
        FastLED.setBrightness(Globals::tvMaxBrightness);
        demoRingActive_ = true;
        ringHue_        = 0;
        timers.create(120, 0, cb_demoRingCh06);
        return;
    }

    StatusFlags::setDemoBit(STATUS_DEMO_PAT_SHIFT, ch.flags & CF_SHIFT_PATTERN);
    StatusFlags::setDemoBit(STATUS_DEMO_COL_SHIFT,  ch.flags & CF_SHIFT_COLORS);

    uint8_t pat = ch.patternId;
    uint8_t col = ch.colorId;

    if (ch.flags & CF_DYN_COLOR) {
        const auto& t = ContextController::time();
        if (ch.ttsFile == 18) {
            float avg = t.hasWeather ? (t.weatherMinC + t.weatherMaxC) / 2.0f : 15.0f;
            col = selectColorForTemp(avg);
        } else if (ch.ttsFile == 19) {
            col = selectColorForDaypart();
        }
    }
    if (pat != 0) LightRun::applyPattern(pat);
    if (col != 0) LightRun::applyColor(col);
}

void cb_demoChapterStart() {
    if (!Globals::demoActive) return;

    if (Globals::demoChapterIdx >= CHAPTER_COUNT) {
        DemoRun::stop();
        return;
    }

    const Chapter& ch = chapters[Globals::demoChapterIdx];

    PF("[Demo] ch%u/%u tts=%u audio=%u pat=%u col=%u flags=0x%02x\n",
       Globals::demoChapterIdx + 1, CHAPTER_COUNT,
       ch.ttsFile, ch.audioFile, ch.patternId, ch.colorId, ch.flags);

    // CF_TV_START: play TTS announcement, then enter TV mode
    if (ch.flags & CF_TV_START) {
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
        uint32_t delay = currentTtsMs_ + 1000 + AUDIO_DELAY_BASE_MS;
        if (delay < MIN_STEP_MS) delay = MIN_STEP_MS;
        timers.create(delay, 1, cb_demoTvEnter);
        return;
    }

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

    // Light change after LIGHT_DELAY_BASE_MS from TTS start
    timers.create(LIGHT_DELAY_BASE_MS, 1, cb_demoChapterLight);
}

void cb_demoChapterLight() {
    if (!Globals::demoActive) return;
    const Chapter& ch = chapters[Globals::demoChapterIdx];

    // Ch1: flash renderer
    if (ch.ttsFile == 1) {
        startFlashSequence(5);
    } else {
        applyChapterLight();
    }

    // Schedule audio after TTS fragment busy window (currentTtsMs_ + 1000) plus gap
    uint32_t remaining = currentTtsMs_ + AUDIO_DELAY_BASE_MS;
    if (remaining < MIN_STEP_MS) remaining = MIN_STEP_MS;
    timers.create(remaining, 1, cb_demoChapterAudio);
}

void cb_demoChapterAudio() {
    if (!Globals::demoActive) return;
    const Chapter& ch = chapters[Globals::demoChapterIdx];

    uint32_t audioMs = scaledAudioMs(ch.audioMaxMs);
    if (audioMs < MIN_STEP_MS) audioMs = MIN_STEP_MS;

    // CF_LIVE_TTS: skip chapter if file not yet rendered
    if (ch.flags & CF_LIVE_TTS) {
        const char* path = getMP3Path(Globals::demoDir, ch.audioFile);
        if (!SD.exists(path)) {
            PF("[Demo] live-TTS %u/%03u missing, skip\n",
               Globals::demoDir, ch.audioFile);
            timers.create(MIN_STEP_MS, 1, cb_demoChapterDone);
            return;
        }
    }

    AudioFragment frag = {};
    frag.dirIndex   = Globals::demoDir;
    frag.fileIndex  = ch.audioFile;
    frag.startMs    = 0;
    frag.durationMs = audioMs;
    frag.fadeMs     = 200;
    strncpy(frag.source, "demo-audio", sizeof(frag.source) - 1);
    PlayAudioFragment::start(frag);

    timers.create(audioMs, 1, cb_demoChapterDone);
}

void cb_demoChapterDone() {
    if (!Globals::demoActive) return;

    // Cleanup ring/flash
    timers.cancel(cb_demoRingCh06);
    timers.cancel(cb_demoFlash);
    if (demoRingActive_) {
        Globals::tvMode = false;
        demoRingActive_ = false;
    } else if (Globals::tvMode) {
        RunManager::exitTvMode();
    }

    // Clear shift bits
    StatusFlags::setDemoBit(STATUS_DEMO_PAT_SHIFT, false);
    StatusFlags::setDemoBit(STATUS_DEMO_COL_SHIFT,  false);

    Globals::demoChapterIdx++;

    // 1ms delay breaks the call stack — no recursion
    timers.create(1, 1, cb_demoChapterStart);
}

void cb_demoInit() {
    if (!Globals::demoActive) return;
    RunManager::requestSetDemoVolume();
    setBrightnessShiftedHi(Globals::brightnessHi);
    // Fire immediately: prtClock values are live, gives TtsTodoQueue max render time before ch17
    timers.create(1, 1, cb_demoInitLiveTts);
    timers.create(1, 1, cb_demoChapterStart);
}

}  // anonymous namespace

namespace DemoRun {

void start() {
    if (Globals::demoActive) return;

    // Stop any running audio and error-flash before taking over lights/audio
    RunManager::requestStopAudio();
    AlertRGB::stopFlashing();

    Globals::demoActive     = true;
    Globals::demoChapterIdx = 0;

    // Compute audio scale factor
    uint32_t total = MathUtils::clamp(Globals::demoTotalMs, 30000UL, 900000UL);
    audioFactor_   = MathUtils::clamp(
        static_cast<float>(total) / static_cast<float>(NATURAL_TOTAL_MS),
        0.10f, 3.00f);

    PF("[Demo] start (%u chapters, total=%lums, factor=%.2f)\n",
       CHAPTER_COUNT,
       static_cast<unsigned long>(Globals::demoTotalMs),
       static_cast<double>(audioFactor_));
    // Defer SD I/O to timer callback (web handler must not touch SD)
    timers.create(1, 1, cb_demoInit);
}

void stop() {
    Globals::demoActive = false;
    timers.cancel(cb_demoChapterStart);
    timers.cancel(cb_demoChapterLight);
    timers.cancel(cb_demoChapterAudio);
    timers.cancel(cb_demoChapterDone);
    timers.cancel(cb_demoFlash);
    timers.cancel(cb_demoRingCh06);
    timers.cancel(cb_demoTvEnter);
    timers.cancel(cb_demoInitLiveTts);
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



