/**
 * @file LightController.cpp
 * @brief LED control implementation via FastLED library
 * @version 251231E
 * @date 2025-12-31
 *
 * Implementation of the LightController class for controlling addressable LED strips
 * using the FastLED library. Handles brightness adjustment, color management,
 * measurement enable for sensor calibration, and visual feedback patterns.
 * Provides centralized LED hardware control with support for various display presets.
 */

#include <Arduino.h>
#include "Globals.h"
#include "LightController.h"
#include <FastLED.h>
//#include <math.h>
#include "AudioState.h"
#include "MathUtils.h"
#include "SensorController.h"
#include "TimerManager.h"

LightController lightController;

// === Measurement Enable ===
// When enabled, all LEDs are forced off for sensor measurement
// RunManager will coordinate this
void LightController::setMeasurementEnabled(bool enable) {
  measurementEnabled = enable;
  if (enable) {
    // Turn off all LEDs for measurement
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    FastLED.setBrightness(0);
    FastLED.show();
  } else {
    // Restore previous brightness and state
    FastLED.setBrightness(getBrightnessShiftedHi());
    updateLightController();
  }
}

bool LightController::isMeasurementEnabled() const {
  return measurementEnabled;
}

namespace {

// Brightness terms used below:
// - Globals::minBrightness/maxBrightness: hardware clamp (never fully off)
// - Globals::brightnessLo/brightnessHi: operational range for slider mapping
// - brightnessUnshiftedHi: base hi boundary before shifts
// - brightnessShiftedHi:   hi boundary after shifts + webShift
// - webShift:              user brightness multiplier (can be > 1.0)
float   webShift = 1.0f;
uint8_t brightnessUnshiftedHi = 100;
uint8_t brightnessShiftedHi = 100;

} // namespace

// WebShift: user brightness multiplier
float getWebShift() {
  return webShift;
}

void setWebShift(float value) {
  webShift = value;  // No clamp - can be >1.0
}

// SliderPct: current shiftedHi as percentage of Lo..Hi range
int getSliderPct() {
  return static_cast<int>(MathUtils::mapRange(
    static_cast<float>(brightnessShiftedHi),
    static_cast<float>(Globals::brightnessLo), static_cast<float>(Globals::brightnessHi),
    static_cast<float>(Globals::loPct), static_cast<float>(Globals::hiPct)));
}

uint8_t getBrightnessShiftedHi() {
  return brightnessShiftedHi;
}

void setBrightnessShiftedHi(float value) {
  brightnessShiftedHi = static_cast<uint8_t>(constrain(value, 0, 255));
}

uint8_t getBrightnessUnshiftedHi() {
  return brightnessUnshiftedHi;
}

void setBrightnessUnshiftedHi(uint8_t value) {
  brightnessUnshiftedHi = value;
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
  applyBrightness();

  float baseRadius = showParams.radius;
  float radiusOsc  = showParams.radiusOsc;

  float animRadius = baseRadius;
  if (radiusOsc != 0.0f) {
    float osc = brightPhase / 255.0f;
    if (radiusOsc > 0.0f) {
      animRadius += fabsf(radiusOsc) * sinf(osc * MathUtils::k2Pi * showParams.gradientSpeed);
    } else {
      animRadius = -showParams.fadeWidth + fabsf(radiusOsc) * osc;
    }
  }

  float centerX = showParams.centerX, centerY = showParams.centerY;
  if (showParams.xAmp != 0.0f) {
    float px = xPhase / 255.0f;
    centerX += showParams.xAmp * sinf(px * MathUtils::k2Pi);
  }
  if (showParams.yAmp != 0.0f) {
    float py = yPhase / 255.0f;
    centerY += showParams.yAmp * sinf(py * MathUtils::k2Pi);
  }

  generateColorGradient(showParams.RGB1, showParams.RGB2, colorGradient, GRADIENT_SIZE);

  // Sliding window over the color gradient: windowStart scrolls through,
  // windowWidth determines how many gradient colors are visible at once
  int windowWidth       = showParams.windowWidth > 0 ? showParams.windowWidth : 16;
  uint8_t windowStart   = colorPhase;
  uint8_t maxBrightness = getBrightnessUnshiftedHi();

  for (int i = 0; i < NUM_LEDS; ++i) {
    LEDPos pos = getLEDPos(i);
    float dx = pos.x - centerX;
    float dy = pos.y - centerY;
    float dist = sqrtf(dx * dx + dy * dy);

    float blend = MathUtils::clamp(fabsf(dist - animRadius) / showParams.fadeWidth, 0.0f, 1.0f);

    float fade = 1.0f - blend;
    fade = fade * fade;

    int gradIdx = (windowStart + int(blend * (windowWidth - 1))) % GRADIENT_SIZE;
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
  // sliderPct is derived from shiftedHi, which already includes webShift
  int sliderPct = getSliderPct();
  
  uint8_t brightness = static_cast<uint8_t>(MathUtils::mapRange(
    static_cast<float>(sliderPct),
    static_cast<float>(Globals::loPct), static_cast<float>(Globals::hiPct),
    static_cast<float>(Globals::brightnessLo), static_cast<float>(Globals::brightnessHi)));
  
  // Audio modulation (only attenuates)
  if (isAudioBusy()) {
    uint16_t audioLevel = getAudioLevelRaw();
    if (audioLevel) {
      float audioFactor = MathUtils::clamp01(sqrtf(audioLevel / 32768.0f) * 1.2f);
      brightness = static_cast<uint8_t>(brightness * audioFactor);
    }
  }

  FastLED.setBrightness(brightness);
}

// === RGB/Helpers ===
void generateColorGradient(const CRGB &colorA, const CRGB &colorB, CRGB *grad, int n) {
  for (int i = 0; i < n; ++i) {
    float t = (float)i / (float)(n - 1);
    uint8_t blend;
    if (t < 0.5f) {
      blend = (uint8_t)(t * 2.0f * 255.0f);
      grad[i] = CRGB(
        lerp8by8(colorA.r, colorB.r, blend),
        lerp8by8(colorA.g, colorB.g, blend),
        lerp8by8(colorA.b, colorB.b, blend));
    } else {
      blend = (uint8_t)((1.0f - (t - 0.5f) * 2.0f) * 255.0f);
      grad[i] = CRGB(
        lerp8by8(colorA.r, colorB.r, blend),
        lerp8by8(colorA.g, colorB.g, blend),
        lerp8by8(colorA.b, colorB.b, blend));
    }
  }
}

void LightController::showOtaPattern() {
  fill_solid(leds, NUM_LEDS, CRGB::OrangeRed);
  FastLED.setBrightness(getBrightnessShiftedHi());
  FastLED.show();
}
