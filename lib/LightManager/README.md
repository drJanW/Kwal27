# LightController Struct-API Architecture

> Version: 251218A | Updated: 2025-12-17

## Pattern Overview

Elke LightShow gebruikt:

Universele struct: LightShowParams (kleur, cycli, type)

Show-specifieke struct: ExtraXXParams (alleen in LightController.h)

Dispatchers: PlayXXShow() in LightController.cpp

Geen eigen timers, geen millis(), geen struct-definities in show-headers

Nieuwe LightShow toevoegen: Werkwijze

Kopieer EmptyShow.cpp en EmptyShow.h als basis. Hernoem naar jouw show (bv FireflyShow).

Maak een struct ExtraFireflyParams in LightController.h

Voeg aan enum LightShow in LightController.h je nieuwe show toe.

Voeg dispatcher toe in LightController.h: void PlayFireflyShow(const LightShowParams&, const ExtraFireflyParams& = dummyFireflyParams);

Voeg include toe in LightController.h: #include "FireflyShow.h"

Implementeer dispatcher in LightController.cpp (zie voorbeeld)

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
- RunManager: `requestLuxMeasurement()` turns LEDs off, waits ~120 ms, triggers `SensorController::performLuxMeasurement()`, then calls `updateBaseBrightness()` before restoring LEDs.
- LightController: `updateBaseBrightness()` uses an exponential mapper: b = b_min + (b_max - b_min) * (1 - exp(-beta * lux)); defaults b_min=20, b_max=220, beta=0.01, lux clamped to 800.
- Logging: When `LOCAL_LOG_LEVEL >= LOG_LEVEL_INFO`, each recalculation prints `[Lux->Brightness] lux=... base=... (beta=...)`.
- Tuning: raise beta for more low-light sensitivity; adjust b_min/b_max for floor/cap; raise L_MAX if your sensor reports higher lux.