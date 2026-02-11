/**
 * @file LightBoot.cpp
 * @brief LED show one-time initialization implementation
 * @version 260201A
 $12026-02-10
 */
#include "LightBoot.h"
#include "LightController.h"
#include "LEDMap.h"
#include "TimerManager.h"
#include "Globals.h"
#include "WebGuiStatus.h"
#include <FastLED.h>

namespace {

void cb_updateLightController() { updateLightController(); }

// Initialize LED hardware and timers
void initLight() {
    FastLED.addLeds<LED_TYPE, PIN_RGB, LED_RGB_ORDER>(leds, NUM_LEDS);
    FastLED.setMaxPowerInVoltsAndMilliamps(MAX_VOLTS, Globals::maxMilliamps);
    FastLED.setBrightness(Globals::maxBrightness);

    if (!loadLEDMapFromSD("/ledmap.bin")) {
        PF("[LightBoot] LED map fallback active\n");
    }

    // LED update timer 50ms (20 FPS)
    timers.create(50, 0, cb_updateLightController);
    timers.create((10 * 1000UL) / 255UL, 0, cb_colorCycle);
    timers.create((10 * 1000UL) / 255UL, 0, cb_brightCycle);
}

} // namespace

void LightBoot::plan() {
    initLight();
    hwStatus |= HW_RGB;
    setBrightnessUnshiftedHi(150);  // Boot default (before shifts)
    setBrightnessShiftedHi(100);    // Initially equal
    setWebShift(1.0f);              // F9: neutral multiplier
    WebGuiStatus::pushState();  // Push initial brightness
}
