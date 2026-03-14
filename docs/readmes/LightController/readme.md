# LightController Struct-API Architecture

> Version: 260307C | Updated: 2026-03-14

## Pattern Overview

Elke LightShow gebruikt:

Universele struct: LightShowParams (kleur, cycli, type)

Show-specifieke struct: ExtraXXParams (alleen in LightController.h)

Play entry points: PlayXXShow() in LightController.cpp

Geen eigen timers, geen millis(), geen struct-definities in show-headers

Nieuwe LightShow toevoegen: Werkwijze

Kopieer EmptyShow.cpp en EmptyShow.h als basis. Hernoem naar jouw show (bv FireflyShow).

Maak een struct ExtraFireflyParams in LightController.h

Voeg aan enum LightShow in LightController.h je nieuwe show toe.

Voeg play entry point toe in LightController.h: void PlayFireflyShow(const LightShowParams&, const ExtraFireflyParams& = dummyFireflyParams);

Voeg include toe in LightController.h: #include "FireflyShow.h"

Implementeer play entry point in LightController.cpp (zie voorbeeld)

Voeg aan switch/case van updateLightController() en PlayLightShow() jouw show toe.

Skeleton: EmptyShow.h

#ifndef EMPTYSHOW_H
#define EMPTYSHOW_H

#include "LightController.h"
#include <FastLED.h>

extern CRGB leds[];

void initEmptyShow(const LightShowParams& p, const ExtraEmptyParams& extra = dummyEmptyParams);
void updateEmptyShow();

#endif

Skeleton: EmptyShow.cpp

#include "EmptyShow.h"

static ExtraEmptyParams emptyParams = {};
const ExtraEmptyParams dummyEmptyParams = ExtraEmptyParams();

void initEmptyShow(const LightShowParams& p, const ExtraEmptyParams& extra) {
emptyParams = extra;
// Initialisatiecode hier
}

void updateEmptyShow() {
// Gebruik cycli via getColorPhase() / getBrightPhase()
for (int i = 0; i < NUM_LEDS; i++) {
leds[i] = CRGB::Black;
}
FastLED.show();
}

Declaratie/aanroepen

In LightController.h:

struct ExtraEmptyParams { ... } // ALLEEN hier!

enum LightShow { ..., emptyShow }

void PlayEmptyShow(const LightShowParams&, const ExtraEmptyParams& = dummyEmptyParams);

#include "EmptyShow.h"

In LightController.cpp:

void PlayEmptyShow(const LightShowParams&, const ExtraEmptyParams&)

case emptyShow: updateEmptyShow(); break; // in updateLightController()

case emptyShow: PlayEmptyShow(p); break; // in PlayLightShow()

Samengevat:

Structs/enums nooit in show-headers

Alleen functieprototypes in show-headers

Geen eigen timers in shows, alles via centrale cycli

Test alles na toevoegen

Dit skelet voorkomt spaghetti en houdt alles schaalbaar en onderhoudbaar.

Ambient lux coordination
- **Sensor**: VEML7700 (replaced BH1750 in v260215+)
- RunManager: `requestLuxMeasurement()` turns LEDs off, waits ~120 ms, triggers `SensorController::performLuxMeasurement()`, then calls `updateBaseBrightness()` before restoring LEDs.
- **LuxCalibration** (v260313D): Gauss-Newton fit engine replaces the simple exponential mapper. Model: `brightness = brMax × (1 - exp(-luxRate × lux))`. Defaults: brMax=222.0, luxRate=0.02. User-trainable via WebGUI calibration panel. See `lib/RunManager/Light/LuxCalibration.h`.
- Logging: When `LOCAL_LOG_LEVEL >= LOG_LEVEL_INFO`, each recalculation prints `[Lux->Brightness] lux=... base=... (beta=...)`.

## TvShow - TV Simulator Renderer (v260307C)

Ring-based TV simulator for the 6 concentric PMMA ring zones of the jellyfish sculpture:

- **6 zones** (0-5, innermost to outermost): 8→16→24→32→36→44 LEDs per ring (160 total)
- `setTvZoneTargets(targets[6])` — set target colors per ring
- `updateTvShow()` — lerp-based smooth rendering (hard cuts or gradual fades)
- Controlled via `tv.js` in the WebGUI

Files: `TvShow.h`, `TvShow.cpp`