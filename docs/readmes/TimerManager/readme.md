# TimerManager Library

> Version: 260206A | Updated: 2026-02-06

TimerManager is a lightweight timer system for Arduino-based ESP32 projects.
It allocates up to 60 software timers that run callbacks at fixed or growing intervals.

- **Non-blocking**: `update()` polls timers from `loop()` without delaying code.
- **Flexible repeats**: run once, N times, or indefinitely.
- **Growing interval**: explicit `growth` parameter enables retry-with-backoff pattern.
- **Callback context**: `remaining()` available inside callbacks — no polling needed.
- **Callback-based**: timers invoke plain `void()` functions.
- **Diagnostics**: inspect available timers or dump active timers for debugging.

This folder contains the reusable library while `src/main.cpp` demonstrates usage.

## Getting Started

1. Include the header and reference the global instance:

```cpp
#include "TimerManager.h"
extern TimerManager timers;
```

2. Define callback functions (e.g. with your `cb_type` macro) that match
`void callback();`.

3. Create timers in `setup()` (or wherever appropriate) and repeatedly call
`timers.update();` inside `loop()`.

## API Overview

```cpp
bool create(uint32_t intervalMs, uint8_t repeat, TimerCallback cb, float growth = 1.0f, uint8_t token = 1);
bool restart(uint32_t intervalMs, uint8_t repeat, TimerCallback cb, float growth = 1.0f, uint8_t token = 1);
void cancel(TimerCallback cb, uint8_t token = 1);
bool isActive(TimerCallback cb, uint8_t token = 1) const;
uint8_t remaining() const;  // valid inside callbacks: remaining repeat count
void update();              // call frequently (usually once per loop)
void showAvailableTimers(bool showAlways);
```

### create() vs restart()

**IMPORTANT**: `create()` returns `false` if a timer with the same callback already exists.
Use `restart()` when the timer may already exist (e.g., sequence callbacks reused with different durations).

```cpp
// Wrong: create() fails silently if timer exists
timers.create(1000, 1, cb_sequenceStep);  // May fail!

// Correct: restart() works whether timer exists or not
timers.restart(1000, 1, cb_sequenceStep);  // Always succeeds
```

### Parameters

- `intervalMs`: interval in milliseconds (initial interval when using growth)
- `repeat`: execution count:
  - `0` = infinite repeats
  - `1` = one-shot
  - `2-255` = exactly N fires
- `cb`: pointer to a function with signature `void callback();`
- `growth`: interval multiplier per fire (default 1.0 = constant, >1.0 = backoff, capped at 20 hours)
- `token`: identity token (default 1). Use different tokens for multiple timers with same callback.

### remaining()

Inside a timer callback, `timers.remaining()` returns the repeat count (0 = infinite, 1 = last fire, >1 = fires left). No need to poll with a callback pointer.

### Repeat Summary

| repeat | growth | Type | Use Case |
|--------|--------|------|----------|
| `0` | `1.0` | Infinite fixed | Heartbeat, polling |
| `1` | `1.0` | One-shot | Delayed action |
| `N` | `1.0` | N times fixed | Limited repeats |
| `N` | `>1.0` | N times growing | Retry with backoff |

## Examples

### 1. One-Shot Notification

```cpp
static void cb_oneShot()
{
	Serial.println("One-shot!" );
}

void setup()
{
	Serial.begin(115200);
	timers.create(5000, 1, cb_oneShot);  // run once after 5 seconds
}

void loop()
{
	timers.update();
}
```

### 2. Retry with Growing Interval

```cpp
static void cb_sensorInit()
{
	if (initSensor()) {
		Serial.println("Sensor ready!");
		timers.cancel(cb_sensorInit);  // Success - stop retrying
		return;
	}
	// Timer continues with growing intervals: 500, 750, 1125, 1687, ...
	Serial.println("Sensor init failed, will retry...");
}

void setup()
{
	Serial.begin(115200);
	// 99 attempts, starting at 500ms, interval grows 1.5x each time (max 30s)
	timers.create(500, 99, cb_sensorInit, 1.5f);
}

void loop()
{
	timers.update();
}
```

### 3. Limited Repeated Task

```cpp
static uint8_t remaining = 3;

static void cb_threeTimes()
{
	Serial.printf("Remaining: %u\n", remaining);
	if (remaining == 0) {
		Serial.println("Done!");
	}
}

void setup()
{
	Serial.begin(115200);
	timers.create(2000, remaining + 1, cb_threeTimes); // 3 repeats + final message
}

void loop()
{
	timers.update();
}
```

### 4. Infinite Heartbeat with Cancel

```cpp
static void cb_heartbeat()
{
	Serial.println("tick");
}

void setup()
{
	Serial.begin(115200);
	timers.create(1000, 0, cb_heartbeat);  // infinite repeats
}

void loop()
{
	static bool stopped = false;
	if (!stopped && millis() > 10000) {
		timers.cancel(cb_heartbeat);
		stopped = true;
	}
	timers.update();
}
```

### 5. Diagnostics Helpers

```cpp
void setup()
{
	Serial.begin(115200);
	timers.create(1000, 0, cb_secondTick);
	timers.create(30000, 0, cb_availableTimers); // prints free timers when triggered
}

void loop()
{
	timers.update();

	// Optionally dump active timers on demand
	if (Serial.available() && Serial.read() == 'd') {
		timers.dump();
	}
}
```

### 6. Build-time Diagnostics Flag

Set `SHOW_TIMER_STATUS=1` in `globals.h` to enable periodic timer status logging. This helps diagnose timer exhaustion or leaks during development.

## Best Practices

- Keep callbacks short—heavy work belongs elsewhere.
- Avoid blocking delays inside callbacks; they run in the main loop context.
- Reuse callbacks sparingly: TimerManager enforces one timer per callback pointer.
- Track `bool` flags if you need to know whether `create()` succeeded.
- Call `update()` as often as possible; typically once each iteration of `loop()`.
- Use growth parameter for hardware init retries—avoids flooding logs with failures.
- Cancel the timer from within the callback when retry succeeds.

## Growing Interval Details

When `growth > 1.0f` (e.g., 1.5f):
- First run: after `intervalMs`
- Second run: after `intervalMs × growth`
- Third run: after `intervalMs × growth²`
- ...continues until repeat count exhausted or callback cancels timer
- Interval capped at 30 seconds to prevent excessively long waits

Example timing for `create(500, 10, cb, 1.5f)`:
```
Attempt 1:     500ms
Attempt 2:     750ms
Attempt 3:   1,125ms
Attempt 4:   1,687ms
Attempt 5:   2,531ms
Attempt 6:   3,796ms
Attempt 7:   5,695ms
Attempt 8:   8,542ms
Attempt 9:  12,814ms
Attempt 10: 19,221ms
```

## Troubleshooting

- **Timer not running**: ensure the callback pointer is unique and `update()` is invoked.
- **No free timers**: `showAvailableTimers(true)` helps log remaining capacity.
- **Need captures/state**: store state in globals or `static` variables; callbacks must be `void()` functions.
- **Growing interval not working**: verify growth > 1.0f (e.g., `1.5f` not `1.0f`).

## License

See the root project for overall licensing. This library follows the same terms unless otherwise noted.
