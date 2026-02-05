/**
 * @file LightController.h
 * @brief LED control interface via FastLED library
 * @version 260202A
 * @date 2026-02-02
 */
#pragma once

#include <FastLED.h>
#include "Globals.h"
#include "LEDMap.h"
#include "TimerManager.h"

class LightController {
public:
  LightController() = default;

  void showOtaPattern();

  // Measurement enable API
  void setMeasurementEnabled(bool enable);
  bool isMeasurementEnabled() const;

private:
  bool measurementEnabled = false;
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

   LightShowParams() = default;

  constexpr LightShowParams(
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
// WebShift: user brightness multiplier (can be >1.0 to override other shifts)
float getWebShift();
void setWebShift(float value);
// SliderPct: current brightness as percentage of Lo..Hi range
int getSliderPct();
// Brightness boundaries
uint8_t getBrightnessShiftedHi();
void setBrightnessShiftedHi(float value);
uint8_t getBrightnessUnshiftedHi();
void setBrightnessUnshiftedHi(uint8_t value);

void updateLightController();
void PlayLightShow(const LightShowParams&);
LightShowParams MakeSolidParams(CRGB color);

// Timer callbacks (used by LightBoot)
void cb_colorCycle();
void cb_brightCycle();

void applyBrightness();
void generateColorGradient(const CRGB& colorA, const CRGB& colorB, CRGB* gradient, int n = GRADIENT_SIZE);
