===========================================================
SYSTEM ARCHITECTURE OVERVIEW
Version: 251218A | Updated: 2025-12-17
===========================================================

INPUT SOURCES
-------------
- Web interface routes (HTTP requests)
- TimerManager callbacks
- Calendar/context refresh
- Sensors (distance, lux, temp, voltage)
- OTA arming/confirm signals
- Manual GPIO inputs

STACK SUMMARY
-------------
Input → Boot → Run → Director → Policy → Controller → Hardware

FLOW OF CONTROL
----------------
1. Input source generates a request
  (e.g. "play fragment", "say time", "silence for 2h", "arm OTA")

2. **Boot layer** (BootManager, *Boot.cpp)
   - Registers timers, seeds RNG/clock, hands pointers into runners.
   - Logs readiness using `PL("[Run][Plan] ...")`.

3. **Run layer** (RunManager, AudioRun, LightRun, ...)
   - Receives requests, owns timer lifecycles, invokes directors, retries on failure.
   - Only this layer calls `TimerManager::restart()` or `TimerManager::cancel()`.

4. **Director layer** (*Director.cpp)
   - Pulls context (calendar rows, SD weights, sensor caches) and assembles a plan.
   - Directors never touch hardware or timers; they simply feed policies.

5. **Policy layer** (*Policy.cpp)
   - Arbitrates requests, clamps values, chooses intervals, exposes helper queries.
   - Returns either APPROVED + params or REJECTED + reason. No side effects.

6. **Controller layer** (AudioManager, LightController, SDController, OTAController, WiFiController, SensorController)
   - Owns runtime state machines and is the only code that talks to drivers (FastLED, I2S, SPI, Wi-Fi, etc.).

7. **Hardware drivers**
   - Perform the physical work (audio output, LEDs, SD, sensors, GPIO).

LAYER PLAYBOOK
--------------
- Boot prepares dependencies → Run sequences work → Director gathers facts → Policy decides → Controller executes.
- Stick to this order for _every_ subsystem. No shortcuts, even for "tiny" features.

EXAMPLE CHAINS
--------------
- **Web silence control**: Web request → WebRun → WebDirector (parse duration + context) → WebPolicy (approve clamp) → AudioRun applies via AudioPolicy/AudioManager.
- **Calendar default lights**: Calendar tick → CalendarRun (no show) → LightDirector builds deterministic `LightShowParams` → LightPolicy approves → LightRun arms timers and calls LightController.
- **Distance PCM**: SensorsRun caches measurements → AudioRun schedules timer → AudioDirector picks clip → AudioPolicy approves volume/interval → AudioManager plays PCM.

STATE + LOGGING
---------------
- Controllers own all mutable state; run/policy/director query through getters only.
- No globals in `Globals.h` for behaviour flags.
- Log every boot/run action with `[Run][Plan]` so bring-up order stays auditable.
- Directors/policies log under their own tags when rejecting or mutating requests.

===========================================================
SUMMARY
===========================================================
- Run = WHEN + sequencing
- Director = WHAT data is available
- Policy = WHAT is allowed
- Controller = HOW it executes
- Hardware = Physical effect
===========================================================