# TimerManager — Non-Blocking Timer Pool

**Role in Kwal27:** Centralized software timer pool that replaces scattered `millis()` polling and `delay()` calls. All subsystem scheduling — light animations, sensor reads, audio sequencing, WiFi retries, NAS backup pushes — flows through this single `timers` global instance.

## Files

| File | Purpose |
|------|---------|
| `TimerManager.h` | Timer pool class definition, `Timer` struct, global `extern` declaration |
| `TimerManager.cpp` | Full implementation including the reentrancy-safe `update()` loop |

## Timer Struct

```cpp
struct Timer {
    bool    active;              // Slot in use?
    TimerCallback cb;            // Callback function pointer (void(*)())
    uint8_t token;               // Identity byte: multiple timers with same callback
    uint32_t interval;           // Current interval in ms (grows with backoff)
    uint32_t nextTime;           // Absolute millis() timestamp for next fire
    uint8_t repeat;              // 0 = infinite, 1 = one-shot, >1 = exact countdown
    float   growthMultiplier;    // 1.0 = constant, >1.0 = exponential backoff per fire
};
```

- **Pool size:** `MAX_TIMERS` (50) defined in `Globals.h` line 29.
- **Growth cap:** `MAX_GROWTH_INTERVAL_MS` (1200 minutes) — backoff intervals cannot exceed this ceiling, preventing runaway intervals in long-running timers.

## Identity Model: (callback, token)

Every timer is identified by its `(cb, token)` pair — not by index. This allows:
- Multiple concurrent timers with the **same callback function** (different tokens)
- Each call to `create()` or `restart()` checks for duplicate `(cb, token)` pairs and rejects them
- `cancel()`, `restart()`, and `isActive()` all use `(cb, token)` identity

```cpp
// Two independent timers, same callback, different tokens:
timers.create(MINUTES(5), 0, cb_checkWeather, 1.0f, 1);  // token 1
timers.create(MINUTES(10), 0, cb_checkWeather, 1.0f, 2); // token 2 — same cb, different token
```

## Complete API

| Method | Signature | Description |
|--------|-----------|-------------|
| `create()` | `(uint32_t interval, uint8_t repeat, TimerCallback cb, float growth = 1.0f, uint8_t token = 1) → bool` | Allocate a timer slot. Returns `false` if pool is full or `(cb, token)` duplicate exists. |
| `cancel()` | `(TimerCallback cb, uint8_t token = 1) → void` | Deactivate timer and reset slot to defaults. Safe to call on non-existent timers. |
| `restart()` | `(uint32_t interval, uint8_t repeat, TimerCallback cb, float growth = 1.0f, uint8_t token = 1) → bool` | Cancel + create in one call. Returns `false` only if pool is full after cancel. |
| `update()` | `() → void` | Called once per `loop()` iteration. Fires due timers (see reentrancy model below). |
| `isActive()` | `(TimerCallback cb, uint8_t token = 1) → bool` | Check if a timer with this identity is currently running. |
| `remaining()` | `() → uint8_t` | Repeat count of the currently-executing callback. **Only valid inside a timer callback.** Returns 0 for infinite timers. |
| `getActiveCount()` | `() → uint8_t` | Returns number of active timers. Also silently updates the peak-usage counter. |
| `getMaxActiveTimers()` | `() → uint8_t` | Returns max simultaneously active timers since boot (diagnostics). |
| `showAvailableTimers()` | `(bool showAlways) → void` | Diagnostics: logs timer usage (current + peak). If `showAlways` is true, always logs; if false, only logs when new peak reached. |

## Reentrancy Model (The Critical Design Detail)

Timer callbacks **may modify their own timer** — cancel it, restart it with different parameters, or replace it entirely. The `update()` loop handles this safely via a snapshot-and-compare pattern:

1. **Before** invoking a callback, `update()` snapshots the timer's: `repeat`, `interval`, `nextTime`, `token`, `growthMultiplier`
2. The callback executes (and may call `cancel()`, `restart()`, etc.)
3. **After** the callback returns, `update()` checks:
   - Is the slot still active? (callback may have cancelled itself)
   - Has the `cb` pointer changed? (callback replaced itself)
   - Has the `token` changed? (slot reused)
   - Have `interval`, `nextTime`, `repeat`, or `growthMultiplier` changed? (callback called `restart()`)
4. If **any change is detected**, `update()` skips rescheduling — the callback already handled the transition
5. If **no change**, `update()` reschedules normally: decrements repeat count, applies growth multiplier, advances `nextTime`

This means `cancel()` + `create()` inside a callback is safe and predates the auto-reschedule logic.

## Repeat Semantics

| `repeat` value | Behavior |
|---------------|----------|
| `0` | Infinite — fires forever at interval |
| `1` | One-shot — fires once, then deactivates |
| `N` (>1) | Fires exactly N times, then deactivates |

Inside a callback, `timers.remaining()` returns the repeat count **before** this fire. For a one-shot (`repeat=1`), `remaining()` returns 1. For infinite (`repeat=0`), `remaining()` returns 0.

## Growth / Backoff

`growthMultiplier > 1.0f` creates exponential backoff — each fire multiplies the interval by this factor. Used for:
- WiFi connection retries (`wifiRetryGrowth = 1.5`)
- Flash burst retries (`flashBurstGrowth = 2.0`)
- Sensor init retries (`distanceSensorInitGrowth = 1.5`)

Intervals are capped at `MAX_GROWTH_INTERVAL_MS` (20 hours) — the timer will not grow beyond this ceiling regardless of `growthMultiplier`.

## Compile-Time Configuration

| Constant | Source | Default | Purpose |
|----------|--------|---------|---------|
| `MAX_TIMERS` | `Globals.h:29` | 50 | Pool capacity |
| `MAX_GROWTH_INTERVAL_MS` | `Globals.h:35` | 1200 minutes | Growth cap |
| `SHOW_TIMER_STATUS` | `Globals.h:32` | `LOG_BOOT_SPAM` | Gates `showAvailableTimers()` output |
| `LOCAL_LOG_LEVEL` | `TimerManager.cpp:25` | `LOG_LEVEL_INFO` | Per-module log verbosity override |

## Macro Helper

```cpp
#define cb_type static void
```

Use `cb_type` when declaring callbacks inside classes/modules to ensure they are plain function pointers (not member functions):

```cpp
class MyModule {
public:
    cb_type cb_onTick();  // expands to: static void cb_onTick();
};
```

## Global Instance

```cpp
extern TimerManager timers;  // declared in TimerManager.h:110, defined in TimerManager.cpp:29
```

All subsystems access the same pool. Typical usage:

```cpp
// One-shot delay
timers.create(5000, 1, cb_delayedAction);

// Infinite repeating (every 6 minutes)
timers.create(MINUTES(6), 0, cb_checkSD);

// Exact 3 fires with backoff
timers.create(1000, 3, cb_retry, 2.0f);

// Cancel by identity
timers.cancel(cb_delayedAction);

// Check inside callback
if (timers.remaining() == 1) { /* last fire */ }