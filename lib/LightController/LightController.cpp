/**
 * @file LightController.cpp
 * @brief LED control implementation via FastLED library
 * @version 260627A
 * @date 2026-06-27
 */
#include <Arduino.h>
#include "Globals.h"
#include "LightController.h"
#include "RingShow.h"
#include <FastLED.h>
#include <math.h>
#include "AudioState.h"
#include "MathUtils.h"
#include "TimerManager.h"

LightController lightController;

namespace {

// Brightness terms used below:
// - Globals::minBrightness/maxBrightness: hardware clamp (never fully off)
// - Globals::brightnessLo/brightnessHi: operational range for slider mapping
// - brightnessBaseHi: base hi boundary before shifts
// - brightnessShiftedHi:   hi boundary after shifts + webMultiplier
// - webMultiplier:         user brightness multiplier (can be > 1.0)
std::atomic<float>   webMultiplier{1.0f};
uint8_t brightnessBaseHi = 100;
std::atomic<uint8_t> brightnessShiftedHi{100};

} // namespace

// WebMultiplier: user brightness multiplier
float getWebMultiplier() {
  return webMultiplier.load(std::memory_order_relaxed);
}

void setWebMultiplier(float value) {
  webMultiplier.store(value, std::memory_order_relaxed);  // No clamp - can be >1.0
}

// SliderPct: current shiftedHi as percentage of Lo..Hi range
int getSliderPct() {
  return static_cast<int>(MathUtils::mapRange(
    brightnessShiftedHi.load(std::memory_order_relaxed),
    Globals::brightnessLo, Globals::brightnessHi,
    Globals::loPct, Globals::hiPct));
}

uint8_t getBrightnessShiftedHi() {
  return brightnessShiftedHi.load(std::memory_order_relaxed);
}

void setBrightnessShiftedHi(float value) {
  brightnessShiftedHi.store(static_cast<uint8_t>(constrain(value, 0, 255)), std::memory_order_relaxed);
}

uint8_t getBrightnessBaseHi() {
  return brightnessBaseHi;
}

void setBrightnessBaseHi(uint8_t value) {
  brightnessBaseHi = value;
}

// === LED buffer ===
CRGB leds[NUM_LEDS];

// === State & Animation for CircleShow ===
static LightShowParams showParams;
static CRGB colorGradient[GRADIENT_SIZE];

static uint8_t xPhase = 0, yPhase = 0;
static uint8_t xCycleSec = 10, yCycleSec = 10;

static uint8_t colorPhase = 0;
static uint8_t brightPhase = 0;

static uint8_t colorCycleSec = 10;
static uint8_t brightCycleSec = 10;

// === Timer callbacks ===
void cb_colorCycle() { colorPhase++; }
void cb_brightCycle() { brightPhase++; }
static void cb_xPhase() { xPhase++; }
static void cb_yPhase() { yPhase++; }

// === Update ===
void updateLightController() {
  if (Globals::isBackgroundSuspended()) { renderRings(); return; }

  applyBrightness();

  // RingShow dispatch — any pattern with pattern_type set uses 6-ring renderer.
  // Patterns never pick their own colors; the assigned color set always
  // provides RGB1→RGB2 via generateColorGradient().
  // Supported types: Rainbow, Blended, Static.
  if (!showParams.patternType.isEmpty()) {
    const String& ptype = showParams.patternType;
    if (ptype.equalsIgnoreCase("RingGradient") || ptype.equalsIgnoreCase("Blended")) {
      // Use user-chosen RGB1→RGB2 gradient from assigned color set
      generateColorGradient(showParams.RGB1, showParams.RGB2, colorGradient, GRADIENT_SIZE);
    }
    updateRingShow(showParams, colorGradient, colorPhase, getBrightnessBaseHi());
    FastLED.show();
    return;
  }

  float baseRadius = showParams.radius;
  float radiusOsc  = showParams.radiusOsc;

  float animRadius = baseRadius;
  if (radiusOsc != 0.0f) {
    float osc = brightPhase / 255.0f;
    if (radiusOsc > 0.0f) {
      animRadius += fabsf(radiusOsc) * sinf(osc * MathUtils::twoPi * showParams.gradientSpeed);
    } else {
      animRadius = -showParams.fadeWidth + fabsf(radiusOsc) * osc;
    }
  }

  float centerX = showParams.centerX, centerY = showParams.centerY;
  if (showParams.xAmp != 0.0f) {
    float px = xPhase / 255.0f;
    centerX += showParams.xAmp * sinf(px * MathUtils::twoPi);
  }
  if (showParams.yAmp != 0.0f) {
    float py = yPhase / 255.0f;
    centerY += showParams.yAmp * sinf(py * MathUtils::twoPi);
  }

  generateColorGradient(showParams.RGB1, showParams.RGB2, colorGradient, GRADIENT_SIZE);

  // Sliding window over the color gradient: windowStart scrolls through,
  // windowWidth determines how many gradient colors are visible at once
  int windowWidth       = showParams.windowWidth > 0 ? showParams.windowWidth : 16;
  uint8_t windowStart   = colorPhase;
  uint8_t maxBrightness = getBrightnessBaseHi();

  for (int i = 0; i < NUM_LEDS; ++i) {
    LEDPos pos = getLEDPos(i);
    float dx = pos.x - centerX;
    float dy = pos.y - centerY;
    float dist = sqrtf(dx * dx + dy * dy);

    float fw = showParams.fadeWidth;
    if (fw < 0.001f) fw = 0.001f;

    // Distance-based blend (existing, always computed)
    float distBlend = MathUtils::clamp(fabsf(dist - animRadius) / fw, 0.0f, 1.0f);

    // Angle-based blend (radial slices: N independent wedges, each with own gradient subrange)
    float angleBlend = 0.0f;
    float angleWeight = showParams.angleWeight;
    int sliceGradientOffset = 0;
    float sliceWidth = showParams.sliceWidth;
    uint8_t sliceCountVal = showParams.sliceCount;

    if (angleWeight > 0.0f && sliceWidth > 0.0f && sliceCountVal > 0) {
        float rawAngle = atan2f(dy, dx);
        float norm     = (rawAngle + M_PI) / (2.0f * M_PI);        // 0..1 full circle

        // Rotate into slice frame (colorPhase scrolls all slices together)
        float sliceRotate = colorPhase / 255.0f;
        float localNorm   = fmodf(norm + sliceRotate, 1.0f);
        if (localNorm < 0.0f) localNorm += 1.0f;

        // Determine which slice this LED belongs to
        float sliceSpan  = 1.0f / (float)sliceCountVal;
        int   sliceIdx   = (int)floorf(localNorm / sliceSpan);
        if (sliceIdx >= sliceCountVal) sliceIdx = sliceCountVal - 1;

        // Center of this slice in localNorm space
        float sliceCenter = ((float)sliceIdx + 0.5f) * sliceSpan;
        float angleDist   = fabsf(localNorm - sliceCenter);
        // Wrap: shortest distance around the circle
        if (angleDist > 0.5f) angleDist = 1.0f - angleDist;

        // angleBlend: 0 at slice center-line, 1 at slice boundary
        float halfW = sliceWidth * 0.5f;
        angleBlend = (halfW > 0.0f) ? MathUtils::clamp(angleDist / halfW, 0.0f, 1.0f) : 0.0f;

        // Each slice gets a different offset into the 256-entry gradient
        sliceGradientOffset = sliceIdx * (GRADIENT_SIZE / sliceCountVal);
    }

    // Lerp between distance and angle blend based on angleWeight
    float blend = (angleWeight > 0.0f)
        ? MathUtils::lerp(distBlend, angleBlend, angleWeight)
        : distBlend;

    float fade = 1.0f - blend;
    fade = fade * fade;

    int gradIdx = (windowStart + sliceGradientOffset + int(blend * (windowWidth - 1))) % GRADIENT_SIZE;
    if (gradIdx < 0) gradIdx += GRADIENT_SIZE;

    CRGB color = colorGradient[gradIdx];

    uint8_t brightness = showParams.minBrightness +
                         uint8_t(fade * (maxBrightness - showParams.minBrightness));
    if (brightness > 0) color.nscale8_video(brightness);
    else                color = CRGB::Black;

    leds[i] = color;
  }

  FastLED.show();
}

LightShowParams MakeSolidParams(CRGB color) {
    return LightShowParams(
        color, color,
        100, 100,          // color/bright cycle (long = no visible cycle)
        64.0f,             // fadeWidth
        222,               // minBrightness (high = bright solid)
        0.0f,              // gradientSpeed (0 = static)
        0.0f, 0.0f,        // centerX, centerY
        0.0f,              // radius
        16,                // windowWidth
        0.0f, 0.0f, 0.0f,  // radiusOsc, xAmp, yAmp (0 = no motion)
        100, 100           // xCycleSec, yCycleSec
    );
}

void PlayLightShow(const LightShowParams &p) {
  showParams = p;
  uint8_t ccs = p.colorCycleSec  > 0 ? p.colorCycleSec  : 10;
  uint8_t bcs = p.brightCycleSec > 0 ? p.brightCycleSec : 10;
  xCycleSec = p.xCycleSec > 0 ? p.xCycleSec : 10;
  yCycleSec = p.yCycleSec > 0 ? p.yCycleSec : 10;

  timers.restart((ccs * 1000UL) / 255UL, 0, cb_colorCycle);
  timers.restart((bcs * 1000UL) / 255UL, 0, cb_brightCycle);
  timers.restart((xCycleSec * 1000UL) / 255UL, 0, cb_xPhase);
  timers.restart((yCycleSec * 1000UL) / 255UL, 0, cb_yPhase);
}

// === Brightness ===
void applyBrightness() {
  // Skip while fade callbacks own FastLED brightness (lux measurement cycle)
  if (Globals::brightnessFading) return;
  // Foreground mode owns brightness — skip normal calculation
  if (Globals::isBackgroundSuspended()) return;

  // sliderPct is derived from shiftedHi, which already includes webMultiplier
  int sliderPct = getSliderPct();
  
  uint8_t brightness = static_cast<uint8_t>(MathUtils::mapRange(
    sliderPct, Globals::loPct, Globals::hiPct,
    Globals::brightnessLo, Globals::brightnessHi));
  
  // Audio modulation (only attenuates)
  if (isAudioBusy()) {
    uint16_t audioLevel = getAudioLevelRaw();
    if (audioLevel) {
      float audioFraction = MathUtils::clamp01(sqrtf(audioLevel / 32768.0f) * 1.2f);
      brightness = static_cast<uint8_t>(brightness * audioFraction);
    }
  }

  FastLED.setBrightness(brightness);
}

// === RGB/Helpers ===
// Linear one-way A→B gradient (replaces old A→B→A palindrome).
// Entry 0 = pure colorA, entry (n-1) = pure colorB.
// Makes CSV rgb1/rgb2 order meaningful: rgb1 sits at blend=0 (bright center),
// rgb2 sits at blend=1 (dark edge).
void generateColorGradient(const CRGB &colorA, const CRGB &colorB, CRGB *grad, int n) {
  for (int i = 0; i < n; ++i) {
    float t = (float)i / (float)(n - 1);
    uint8_t blend = (uint8_t)(t * 255.0f);
    grad[i] = CRGB(
      lerp8by8(colorA.r, colorB.r, blend),
      lerp8by8(colorA.g, colorB.g, blend),
      lerp8by8(colorA.b, colorB.b, blend));
  }
}
