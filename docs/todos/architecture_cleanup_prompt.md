# Architecture Cleanup — Refactor Session

## Context
`tools\check_architecture.ps1` found **38 violations** and **27 warnings** in the codebase.
These are cross-controller includes, cb_* in wrong layers, hardware headers in Policy, and SD I/O in web handlers.
Most were silently introduced by Copilot in past sessions and never caught.

## Goal
Reduce violations to **zero**. Work through the list systematically, one file at a time.
Run `.\tools\check_architecture.ps1` after each batch to confirm the count drops.
Commit after each clean file or logical group.

## Rules
- Read copilot-instructions.md carefully — especially the Architecture and Anti-Shortcut sections.
- NEVER introduce a new violation while fixing another.
- If a fix requires a new shared interface (e.g. moving SDController access behind ContextController), propose it first — don't just move code around.
- Run `.\tools\check_architecture.ps1` after every change to verify the count drops.

## Current violation list (38 violations, 27 warnings)

### VIOLATIONS — Cross-controller includes

**AudioManager → SDController (11)**
- PlayFragment.cpp:17 includes SDController.h
- PlayFragment.cpp:79,236 references SDController namespace
- PlayPCM.cpp:14 includes SDController.h
- PlayPCM.cpp:84,89,101,124,132,140,148,153 references SDController namespace

**AudioManager → WiFiController (1)**
- PlaySentence.cpp:23 includes WiFiController.h

**AudioManager → SDController (1)**
- PlaySentence.cpp:26 includes SDSettings.h (from SDController)

**ClockController → WiFiController (2)**
- PRTClock.cpp:11 includes FetchController.h
- PRTClock.cpp:105 references FetchController namespace

**LightController → SDController (3)**
- LEDMap.cpp:37,41,54 references SDController namespace

**LightController → AudioManager (1)**
- LightController.cpp:12 includes AudioState.h

**LightController → SensorController (1)**
- LightController.cpp:14 includes SensorController.h

**SDController → AudioManager (1)**
- SDVoting.cpp:11 includes AudioState.h

**WiFiController → ClockController (1)**
- FetchController.cpp:11 includes PRTClock.h

**WiFiController → SDController (7)**
- FetchController.cpp:13 includes SDController.h
- FetchController.cpp:456,460,463 references SDController namespace
- NasBackup.cpp:22 includes SDController.h
- NasBackup.cpp:54,55,56,60,61,69,70 references SDController namespace

**WiFiController → AudioManager (2)**
- FetchController.cpp:15 includes AudioState.h
- NasBackup.cpp:24 includes AudioState.h

**Policy → Hardware (1)**
- AlertPolicy.h:10 includes FastLED.h

### WARNINGS — cb_* outside RunManager

**AudioManager (12)**
- AudioManager.cpp:48,110 — cb_audioMeter
- PlayFragment.cpp:63-66,249,269,287,305 — cb_fadeIn, cb_fadeOut, cb_beginFadeOut, cb_fragmentReady
- PlayPCM.cpp:164 — cb_stopPCMPlayback
- PlaySentence.cpp:212 — cb_ttsReady

**LightController (2)**
- LightController.cpp:83,84 — cb_colorCycle, cb_brightCycle

**SensorController (3)**
- SensorController.cpp:65,69,74 — cb_distanceInit, cb_luxInit, cb_luxSensorRead

**WebInterfaceController (3)**
- HealthRoutes.cpp:87 — cb_restart
- OtaRoutes.cpp:23 — cb_rebootAfterOta
- SseController.cpp:34 — cb_deferredPush

**WiFiController (2)**
- NasBackup.cpp:130,147 — cb_pushToNas, cb_checkNasHealth

**Web handlers with SD I/O (5)**
- SdRoutes.cpp:88,195,204,263,319

## Suggested approach
1. Start with **SDController coupling** — it's the biggest offender (22 of 38 violations)
2. Then **AudioState.h cross-includes** (4 violations)
3. Then **ClockController ↔ WiFiController** (3 violations)
4. Then **LightController cross-includes** (4 violations)
5. Then **AlertPolicy.h FastLED** (1 violation)
6. Then tackle cb_* warnings
7. Then SdRoutes web I/O warnings
