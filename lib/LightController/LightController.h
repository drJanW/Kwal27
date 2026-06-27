/**
 * @file LightController.h
 * @brief LED control interface via FastLED library
 * @version 260615E
 * @date 2026-06-04
 */
#pragma once

#include <FastLED.h>
#include "Globals.h"
#include "LEDMap.h"
#include "TimerManager.h"

class LightController {
public:
  LightController() = default;

private:
  LightController(const LightController&) = delete;
  LightController& operator=(const LightController&) = delete;
};

extern LightController lightController;

// ===== ENUMS EN STRUCTS =====
// enum LightShow {    circleShow};

struct LightShowParams {
  CRGB RGB1, RGB2;
  uint8_t colorCycleSec, brightCycleSec, minBrightness, xCycleSec, yCycleSec;
  float fadeWidth, gradientSpeed, centerX, centerY, radius, radiusOsc, xAmp, yAmp;
  int   windowWidth;
  uint8_t id{0};  // Pattern ID from CSV (0 = unset = use default circle renderer)

  // RingShow column (pattern #32)
  String patternType;   // "RingGradient" = 6-ring gradient scroll, empty = CircleShow

  // Radial slice columns (angle blends into existing distance blend)
  float   angleWeight = 0.0f;   // 0 = pure distance (existing), 1 = pure angle, 0..1 = blend
  uint8_t sliceCount  = 1;      // number of radial slices: 1 = single wedge, N = N slices
  float   sliceWidth  = 0.0f;   // angular width per slice as fraction of circle (0..1). 0 = disabled

   LightShowParams() = default;

   LightShowParams(
    CRGB a, CRGB b, uint8_t cCol, uint8_t cBrt, float fW, uint8_t minB, float gS,
    float cx, float cy, float r, int wW, float rOsc, float xA, float yA,
    uint8_t xC, uint8_t yC)
  : RGB1(a), RGB2(b),
    colorCycleSec(cCol), brightCycleSec(cBrt),
    minBrightness(minB),
    xCycleSec(xC), yCycleSec(yC),
    fadeWidth(fW), gradientSpeed(gS),
    centerX(cx), centerY(cy), radius(r),
    radiusOsc(rOsc), xAmp(xA), yAmp(yA),
    windowWidth(wW) {}
};

/*
struct LightShowParams {
    //LightShow type = circleShow;
    CRGB RGB1 = CRGB::LightPink;
    CRGB RGB2 = CRGB::DeepPink;
    uint8_t colorCycleSec = 10;
    uint8_t brightCycleSec = 10;
    float fadeWidth = 8.0f;
    uint8_t minBrightness = 10;
    float gradientSpeed = 5.1f;
    float centerX     = 0.0f;
    float centerY     = 0.0f;
    float radius      = 20.0f;
    int   windowWidth = 16;
    float radiusOsc   = 0.0f;
    float xAmp        = 0.0f;
    float yAmp        = 0.0f;
    uint8_t xCycleSec = 10;
    uint8_t yCycleSec = 10;
};
*/
#define GRADIENT_SIZE 256
extern CRGB leds[];

// ===== API =====
// WebMultiplier: user brightness multiplier (can be >1.0 to override other shifts)
float getWebMultiplier();
void setWebMultiplier(float value);
// SliderPct: current brightness as percentage of Lo..Hi range
int getSliderPct();
// Brightness boundaries
uint8_t getBrightnessShiftedHi();
void setBrightnessShiftedHi(float value);
uint8_t getBrightnessBaseHi();
void setBrightnessBaseHi(uint8_t value);

void updateLightController();
void PlayLightShow(const LightShowParams&);
LightShowParams MakeSolidParams(CRGB color);

// Timer callbacks (used by LightBoot)
void cb_colorCycle();
void cb_brightCycle();

void applyBrightness();
void generateColorGradient(const CRGB& colorA, const CRGB& colorB, CRGB* gradient, int n = GRADIENT_SIZE);
