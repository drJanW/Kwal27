/**
 * @file LightBoot.cpp
 * @brief LED show one-time initialization implementation
 * @version 260219C
 * @date 2026-02-19
 */
#include "LightBoot.h"
#include "LightController.h"
#include "LEDMap.h"
#include "TimerManager.h"
#include "Globals.h"
#include "MathUtils.h"
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
    setBrightnessBaseHi(150);  // Boot default (before shifts)
    setBrightnessShiftedHi(100);    // Initially equal

    // Compute initial webMultiplier from defaultBrightnessSliderPct (Globals/CSV).
    // Maps desired slider% to target brightness, then divides by brightnessHi
    // to get approximate multiplier. Exact slider position depends on lux/calendar
    // shifts which aren't known yet at boot — first lux measurement will refine.
    float targetBri = MathUtils::map(
        Globals::defaultBrightnessSliderPct,
        Globals::loPct, Globals::hiPct,
        (float)Globals::brightnessLo, (float)Globals::brightnessHi);
    float initWebMult = (Globals::brightnessHi > 0)
        ? (targetBri / Globals::brightnessHi)
        : 1.0f;
    setWebMultiplier(initWebMult);
    PF("[LightBoot] Slider=%u → webMultiplier=%.3f\n",
       Globals::defaultBrightnessSliderPct, initWebMult);
    WebGuiStatus::pushState();  // Push initial brightness
}
