/**
 * @file DemoRun.cpp
 * @brief 31-chapter demo orchestrator
 * @version 260605A
 * @date 2026-06-05
 *
 * Per chapter:
 *   Step 0: set pattern+colors, start TTS audio, schedule Step 1 at
 *           (estimated_tts_duration_ms + INTER_PAUSE_MS).
 *   Step 1: start music/sfx fragment with cap audioMaxMs, schedule Step 2.
 *   Step 2: advance to next chapter, restart Step 0. End → stop().
 *
 * Chapters with ttsFile == 0 are skipped quickly (e.g. placeholder 10).
 * Chapters with audioFile == 0 skip Step 1.
 *
 * Duration scaling: total demo length is governed by Globals::demoTotalMs.
 * audioMaxMs of each chapter is scaled by a factor; TTS playback duration
 * is dictated by the MP3 file size and is not scaled.
 */
#include <Arduino.h>
#include <SD.h>

#include "DemoRun.h"
#include "Globals.h"
#include "TimerManager.h"
#include "PlayFragment.h"
#include "Light/LightRun.h"
#include "SdFileAccess.h"     // getMP3Path declared here

namespace {

struct Chapter {
    uint8_t  ttsFile;       // /demoDir/<ttsFile>.mp3 (narration); 0 = skip
    uint8_t  audioFile;     // /demoDir/<audioFile>.mp3 (music/sfx); 0 = none
    uint16_t audioMaxMs;    // Cap for audio fragment
    uint8_t  patternId;     // light_patterns.csv id; 0 = leave current
    uint8_t  colorId;       // light_colors.csv  id; 0 = leave current
};

// Mapping derived from docs/demo_program.txt.
// Pattern/color IDs use existing entries (32/Spectrum was added 260604D).
// Chapter 10 = placeholder (skipped). Chapter 29 = quiet TV-ish placeholder
// for v1 (full tvMode integration deferred).
static const Chapter chapters[] = {
    // tts  aud  audMs  pat col
    {   1,  51,  5000,  28,  5 },  // 01 opening: flits + bel
    {   2,  52,  5000,   2,  3 },  // 02 welkom: groen
    {   3,  53,  5000,  27,  2 },  // 03 inleiding1: blauwe ringen
    {   4,  54,  5000,  27, 40 },  // 04 inleiding2: witte ringen
    {   5,  55,  5000,  27,  2 },  // 05 inleiding3: blauw in wit
    {   6,  56,  8000,  31, 35 },  // 06 extreem zappa
    {   7,  57,  8000,  31,  8 },  // 07 extreem onweer
    {   8,  58,  8000,  31,  4 },  // 08 extreem disco
    {   9,  59, 30000,  32, 32 },  // 09 ringspectrum (NIEUW)
    {   0,   0,  2000,   0,  0 },  // 10 placeholder (skip)
    {  11,  61,  8000,  27, 41 },  // 11 lounge: Polar Lights + Lemon Yellow
    {  12,  62,  8000,   2,  2 },  // 12 ademen: Slow Breathing + Cool Ocean
    {  13,  63,  8000,  27,  2 },  // 13 noorderlicht: Polar Lights + Cool Ocean
    {  14,  64,  8000,   7,  1 },  // 14 gloed: Radiant Glow + Warm Sunset
    {  15,  65,  8000,   8, 29 },  // 15 sterren: Twinkling Stars + Cobalt
    {  16,  66,  8000,   5,  4 },  // 16 rust: Calm Center + Royal Purple
    {  17,  86,  3000,   5, 29 },  // 17 tijd (live TTS 086)
    {  18,  87,  3000,   7,  1 },  // 18 temperatuur (live TTS 087)
    {  19,  88,  4000,   1,  1 },  // 19 zon (live TTS 088)
    {  20,  89,  3000,   5, 40 },  // 20 maan (live TTS 089)
    {  21,  90,  4000,   1, 39 },  // 21 kalender (live TTS 090)
    {  22,  72,  4000,   2,  2 },  // 22 variatie: basis1
    {  23,  73,  4000,   2,  2 },  // 23 variatie: patternShift
    {  24,  74,  4000,   2,  2 },  // 24 variatie: basis2
    {  25,  75,  4000,   2,  2 },  // 25 variatie: colorsShift
    {  26,  76,  4000,   2,  2 },  // 26 variatie: basis3
    {  27,  77,  4000,   2,  2 },  // 27 variatie: beide
    {  28,  78,  4000,   2,  2 },  // 28 variatie: basis4
    {  29,   0,  5000,   2,  3 },  // 29 tv-sim placeholder (no audio)
    {  30,  80,  6000,  28,  5 },  // 30 vuurwerk: Fireworks + Sunny Yellow
    {  31,  81,  8000,   2, 40 },  // 31 afscheid: Slow Breathing + Ice White
};
static constexpr uint8_t CHAPTER_COUNT = sizeof(chapters) / sizeof(chapters[0]);

constexpr uint16_t INTER_PAUSE_MS = 1500;   // Gap between TTS end and audio start
constexpr uint16_t MIN_STEP_MS    = 100;    // Safety floor for timer scheduling

// Reference duration of the natural (un-scaled) demo (sum of audioMaxMs +
// TTS allowance + inter-pauses). Denominator for the audio scale factor.
constexpr uint32_t NATURAL_TOTAL_MS = 360000UL;  // ~6 minutes

static float audioFactor_ = 1.0f;

uint16_t scaledAudioMs(uint16_t raw) {
    if (raw == 0) return 0;
    float v = raw * audioFactor_;
    if (v < 200.0f)   v = 200.0f;
    if (v > 60000.0f) v = 60000.0f;
    return static_cast<uint16_t>(v);
}

// Estimate TTS playback duration in ms from file size on SD.
// Uses the same VoiceRSS formula as PlaySentence::downloadTtsToCache:
//   estMs = (bytes * factor) / 16
// Returns 0 if file missing.
uint32_t estimateTtsMs(uint8_t dir, uint8_t file) {
    const char* path = getMP3Path(dir, file);
    File f = SD.open(path, FILE_READ);
    if (!f) return 0;
    size_t bytes = f.size();
    f.close();
    if (bytes == 0) return 0;
    return static_cast<uint32_t>((bytes * Globals::ttsCacheDurationFactor) / 16);
}

void cb_demoStep1();  // forward
void cb_demoStep2();  // forward
void startStep0();    // forward (regular helper, not a timer cb)

void startStep0() {
    if (!Globals::demoActive) return;
    if (Globals::demoChapterIdx >= CHAPTER_COUNT) {
        DemoRun::stop();
        return;
    }
    const Chapter& ch = chapters[Globals::demoChapterIdx];
    PF("[Demo] chapter %u/%u (tts=%u audio=%u pat=%u col=%u)\n",
       static_cast<unsigned>(Globals::demoChapterIdx + 1),
       static_cast<unsigned>(CHAPTER_COUNT),
       ch.ttsFile, ch.audioFile, ch.patternId, ch.colorId);

    // Apply pattern + colors (0 = leave current)
    if (ch.patternId != 0) LightRun::applyPattern(ch.patternId);
    if (ch.colorId   != 0) LightRun::applyColor(ch.colorId);

    // Start TTS narration if available
    uint32_t ttsMs = 0;
    if (ch.ttsFile != 0) {
        ttsMs = estimateTtsMs(Globals::demoDir, ch.ttsFile);
        if (ttsMs > 0) {
            AudioFragment frag = {};
            frag.dirIndex   = Globals::demoDir;
            frag.fileIndex  = ch.ttsFile;
            frag.startMs    = 0;
            frag.durationMs = ttsMs + 1000;  // small margin
            frag.fadeMs     = 0;
            strncpy(frag.source, "demo-tts", sizeof(frag.source) - 1);
            PlayAudioFragment::start(frag);
        } else {
            PF("[Demo] tts %u/%u missing, skipping narration\n",
               Globals::demoDir, ch.ttsFile);
        }
    }

    uint32_t nextMs = ttsMs + INTER_PAUSE_MS;
    if (nextMs < MIN_STEP_MS) nextMs = MIN_STEP_MS;
    timers.create(nextMs, 1, cb_demoStep1);
}

void cb_demoStep1() {
    if (!Globals::demoActive) return;
    if (Globals::demoChapterIdx >= CHAPTER_COUNT) {
        DemoRun::stop();
        return;
    }
    const Chapter& ch = chapters[Globals::demoChapterIdx];

    if (ch.audioFile == 0 || ch.audioMaxMs == 0) {
        // No music/sfx phase — pause briefly, then advance
        uint16_t scaled = scaledAudioMs(ch.audioMaxMs);
        uint32_t waitMs = scaled > 0 ? scaled : MIN_STEP_MS;
        timers.create(waitMs, 1, cb_demoStep2);
        return;
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
    Globals::demoChapterIdx++;
    startStep0();
}

}  // anonymous namespace

namespace DemoRun {

void start() {
    if (Globals::demoActive) return;
    Globals::demoActive     = true;
    Globals::demoChapterIdx = 0;

    // Compute audio scale factor from requested total duration
    uint32_t total = Globals::demoTotalMs;
    if (total < 30000UL)  total = 30000UL;
    if (total > 900000UL) total = 900000UL;
    audioFactor_ = static_cast<float>(total) / static_cast<float>(NATURAL_TOTAL_MS);
    if (audioFactor_ < 0.10f) audioFactor_ = 0.10f;
    if (audioFactor_ > 3.00f) audioFactor_ = 3.00f;

    PF("[Demo] start (%u chapters, total=%lums, factor=%.2f)\n",
       static_cast<unsigned>(CHAPTER_COUNT),
       static_cast<unsigned long>(total),
       static_cast<double>(audioFactor_));
    startStep0();
}

void stop() {
    if (!Globals::demoActive) {
        // Make stop idempotent — still cancel timers in case of stray state
        timers.cancel(cb_demoStep1);
        timers.cancel(cb_demoStep2);
        return;
    }
    Globals::demoActive = false;
    timers.cancel(cb_demoStep1);
    timers.cancel(cb_demoStep2);
    PlayAudioFragment::stop(500);
    PL("[Demo] stop");
}

bool isActive() { return Globals::demoActive; }

uint8_t chapterCount() { return CHAPTER_COUNT; }

}  // namespace DemoRun
