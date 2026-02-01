# RunManager Architecture

> Version: 251218A | Updated: 2025-12-17

Every subsystem that lives under `lib/RunManager/**` follows the same stack so we can iterate on behaviour without touching boot code, timers, or hardware drivers. The stack is strict—skip a layer and you get bugs that are impossible to reason about later.

## Layer Contract

| Layer | Responsibility | Typical Files |
| --- | --- | --- |
| **Boot** | Register timers, seed caches, hand off any pointers that run/policy will need later. Boot files run exactly once during system bring-up. | `AudioBoot.*`, `LightBoot.*`, `BootMaster.*` |
| **Run** | Orchestrate requests. They own timer callbacks, subscribe to controller signals, and decide which director to invoke. Run code may _sequence_ work but must not invent behaviour. | `*Run.*`, `RunManager.*` |
| **Director** | Translate context + storage into actionable requests. Directors query controllers (state, SD indices, calendar rows) and shape the payload that policies evaluate. They never touch hardware APIs directly. | `*Director.*` |
| **Policy** | Enforce rules for a single domain (audio, light, OTA, etc.). Policies approve/deny requests, clamp levels, pick intervals, and expose helpers such as “distance playback volume”. They cannot schedule timers or manipulate global singletons. | `*Policy.*` |
| **Controller** | Own hardware drivers and runtime state. Controllers expose query/update APIs and keep the state machines honest. | `AudioManager.*`, `LightController.*`, `SDController.*`, ... |
| **Hardware** | Actual drivers (I²S, FastLED, SPI, GPIO). Only controllers touch these.

## Request Flow

1. An input source (web, timers, calendar, sensors) raises a request via `RunManager::request*` or a subsystem-specific entry point.
2. The owning **Run** module validates prerequisites (boot complete, timer slots free) and invokes the matching **Director**.
3. The **Director** builds a request composed of current context (calendar themes, distance cache, OTA arm window, etc.).
4. The **Policy** evaluates that request, returning either approval plus concrete parameters or a rejection reason.
5. If approved, the **Run** code asks the appropriate **Controller** to execute. Controllers update hardware via their drivers and report state back up the stack.

## Boot Discipline

- `BootMaster` coordinates shared start-up (clock seeding, fallback timers) and hands off once common services are alive.
- Each subsystem boot (`AudioBoot`, `LightBoot`, etc.) is responsible for: registering timers with `TimerManager`, caching pointers (e.g. PCM clips), and logging readiness via `PL("[Run][Plan] ...")`.
- Boot layers must never start behaviour directly. They only prepare the scaffolding so that `RunManager::update()` and the subsystem runners can run deterministically.

## Error Notification Behavior

When hardware fails (SD card, sensors, etc.), the system uses `AlertPolicy` and `AlertRGB` to provide visual feedback.

### Hardware Presence Architecture (v260104+)

Hardware can be marked as **absent** in `HWconfig.h` using `*_PRESENT` defines:
- `RTC_PRESENT`, `DISTANCE_SENSOR_PRESENT`, `LUX_SENSOR_PRESENT`, `SENSOR3_PRESENT`
- Absent hardware: no init attempts, no flash, no reminders, status = `—` (not ❌)
- Only hardware marked as present will trigger error handling

### Flash Burst Sequence

One burst = black(1s) + color(1-2s) + black(1s) ≈ 3-4s per failing component.

1. **Boot flash**: 2× bursts immediately when error detected
2. **Reminder flashes**: After boot burst, single flash at growing intervals (2, 20, 200, 2000... min)
3. **Component colors**: Each hardware type has a unique color (defined in `AlertPolicy.h`)

### Status Values (`SC_Status`)

| Value | Meaning | Display |
|-------|---------|--------|
| `OK` | Working | ✅ |
| `RETRY` | Init in progress (2-14 tries left) | ⟳N |
| `LAST_TRY` | Final attempt | ⟳1 |
| `FAILED` | Gave up | ❌ |
| `ABSENT` | Hardware not present | — |

Files: `AlertPolicy.h`, `AlertRGB.cpp`, `AlertState.h`, `AlertState.cpp`

## Logging + Telemetry Rules

- Use `PL("[Run][Plan] ...")` for lifecycle events and runner state changes so the boot log shows every registration in order.
- Directors and policies log through `[AudioDirector]`, `[LightPolicy]`, etc. Keep rejection reasons explicit so runners can surface them upstream without guessing.
- Timer churn stays visible by logging whenever a runner parks or re-arms a slot (especially for distance audio and OTA windows).

## Do / Don’t Checklist

- ✅ Run owns timers, sequencing, retries, and is the only layer that calls `TimerManager::restart`.
- ✅ Directors read data (calendar CSV, SD index, sensors) and return structured plans. They never make policy decisions.
- ✅ Policies accept a plan and spit back either a green-light (with concrete numbers) or “no” with a reason. No side effects besides cached clamp state.
- ✅ Controllers expose explicit APIs for every action; do not reach into singletons or globals directly from run/policy code.
- ✅ Controllers write status to AlertState (or ContextStatus); status reads belong to AlertState (or ContextStatus), not controller APIs.
- ❌ No layer except controllers talks to hardware (`FastLED`, `I2S`, `SPI`, raw GPIO).
- ❌ Policies never schedule timers, mutate controller state directly, or call other policies.
- ❌ Runners do not normalise raw sensor readings—that belongs to the relevant input policy/controller.

Keeping these contracts tight lets us replace a policy, director, or even an entire subsystem without rewriting the rest of the pipeline.
