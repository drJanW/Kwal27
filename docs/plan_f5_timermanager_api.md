# F5: TimerManager API Refactor

## Goal
Replace negative repeat parameter (growing interval hack) with explicit `growth` float parameter.

## Current API

```cpp
// TimerManager.h
struct Timer {
    bool active = false;
    TimerCallback cb = nullptr;
    uint32_t interval = 0;
    uint32_t nextTime = 0;
    int32_t repeat = 0;        // <-- signed, negative = growing
    bool intervalGrows = false; // <-- derived from repeat < 0
};

bool create(uint32_t interval, int32_t repeat, TimerCallback cb);
bool restart(uint32_t interval, int32_t repeat, TimerCallback cb);
```

**Semantics (current):**
- `repeat = 0` → infinite
- `repeat > 0` → N repeats at fixed interval
- `repeat < 0` → |N| repeats with interval growing by `GROWING_MULTIPLIER` (1.5f)

## New API

```cpp
// TimerManager.h
struct Timer {
    bool active = false;
    TimerCallback cb = nullptr;
    uint32_t interval = 0;
    uint32_t nextTime = 0;
    uint8_t repeat = 0;        // <-- unsigned, 0-255
    float growthFactor = 1.0f; // <-- explicit growth multiplier
};

bool create(uint32_t interval, uint8_t repeat, TimerCallback cb, float growth = 1.0f);
bool restart(uint32_t interval, uint8_t repeat, TimerCallback cb, float growth = 1.0f);
```

**Semantics (new):**
- `repeat = 0` → infinite, growth forced to 1.0f
- `repeat = 1` → one-shot
- `repeat = 2-255` → N repeats
- `growth = 1.0f` → constant interval (default)
- `growth > 1.0f` → interval multiplied by growth each run (capped at `MAX_GROWTH_INTERVAL_MS`)

## Files to Change

| File | Change |
|------|--------|
| lib/TimerManager/TimerManager.h | Struct + function signatures |
| lib/TimerManager/TimerManager.cpp | Implementation logic |
| lib/TimerManager/README.md | Update examples |
| lib/Globals/Globals.h | Remove `GROWING_MULTIPLIER` |
| lib/Globals/I2CInitHelper.h | Update struct: `int maxRetries` → `uint8_t maxRetries` + add `float growth` |
| lib/Globals/I2CInitHelper.cpp | Pass growth to create() |
| lib/SensorManager/SensorManager.cpp | Update 2 I2CInitHelper::start() calls |
| lib/ConductManager/Clock/ClockPolicy.cpp | Update 1 I2CInitHelper::start() call |
| lib/WiFiManager/FetchManager.cpp | Update 3 direct create() calls |
| docs/sensor3_implementation_guide.md | Update example |
| docs/plan_i2c_init_refactor.md | Update examples |

## Pseudo Code Changes

### 1. TimerManager.h

```cpp
// BEFORE
struct Timer {
    bool active = false;
    TimerCallback cb = nullptr;
    uint32_t interval = 0;
    uint32_t nextTime = 0;
    int32_t repeat = 0;
    bool intervalGrows = false;
};

bool create(uint32_t interval, int32_t repeat, TimerCallback cb);
bool restart(uint32_t interval, int32_t repeat, TimerCallback cb);
int32_t getRepeatCount(TimerCallback cb) const;

// AFTER
struct Timer {
    bool active = false;
    TimerCallback cb = nullptr;
    uint32_t interval = 0;
    uint32_t nextTime = 0;
    uint8_t repeat = 0;
    float growthFactor = 1.0f;
};

bool create(uint32_t interval, uint8_t repeat, TimerCallback cb, float growth = 1.0f);
bool restart(uint32_t interval, uint8_t repeat, TimerCallback cb, float growth = 1.0f);
int16_t getRepeatCount(TimerCallback cb) const;  // returns -1 if not found
```

### 2. TimerManager.cpp create()

```cpp
// BEFORE
bool TimerManager::create(uint32_t interval, int32_t repeat, TimerCallback cb) {
    // ...find free timer...
    timers[i].repeat = repeat;
    timers[i].intervalGrows = (repeat < 0);
    return true;
}

// AFTER
bool TimerManager::create(uint32_t interval, uint8_t repeat, TimerCallback cb, float growth) {
    // ...find free timer...
    timers[i].repeat = repeat;
    // Force growth = 1.0f for infinite timers
    timers[i].growthFactor = (repeat == 0) ? 1.0f : growth;
    return true;
}
```

### 3. TimerManager.cpp restart()

```cpp
// BEFORE
bool TimerManager::restart(uint32_t interval, int32_t repeat, TimerCallback cb) {
    if (cb) cancel(cb);
    return create(interval, repeat, cb);
}

// AFTER
bool TimerManager::restart(uint32_t interval, uint8_t repeat, TimerCallback cb, float growth) {
    if (cb) cancel(cb);
    return create(interval, repeat, cb, growth);
}
```

### 4. TimerManager.cpp update()

```cpp
// BEFORE (inside update loop after callback runs)
if (originalRepeat == 1 || originalRepeat == -1) {
    // Last repeat - deactivate
    timers[i].active = false;
} else if (originalRepeat != 0) {
    // More repeats: positive counts down, negative counts up towards 0
    timers[i].repeat += (originalRepeat < 0) ? 1 : -1;
    
    if (timers[i].intervalGrows) {
        uint32_t newInterval = (uint32_t)(timers[i].interval * GROWING_MULTIPLIER);
        timers[i].interval = min(newInterval, MAX_GROWTH_INTERVAL_MS);
    }
    timers[i].nextTime += timers[i].interval;
} else {
    // Infinite timer
    timers[i].nextTime += timers[i].interval;
}

// AFTER
if (originalRepeat == 1) {
    // Last repeat - deactivate
    timers[i].active = false;
    timers[i].cb = nullptr;
} else if (originalRepeat > 1) {
    // More repeats - count down
    timers[i].repeat--;
    
    // Apply growth factor
    if (timers[i].growthFactor > 1.0f) {
        uint32_t newInterval = (uint32_t)(timers[i].interval * timers[i].growthFactor);
        timers[i].interval = min(newInterval, MAX_GROWTH_INTERVAL_MS);
    }
    timers[i].nextTime += timers[i].interval;
} else {
    // Infinite timer (repeat == 0)
    timers[i].nextTime += timers[i].interval;
}
```

### 5. TimerManager.cpp constructor

```cpp
// BEFORE
TimerManager::TimerManager() {
    for (uint8_t i = 0; i < MAX_TIMERS; i++) {
        timers[i].active = false;
        timers[i].intervalGrows = false;
    }
}

// AFTER
TimerManager::TimerManager() {
    for (uint8_t i = 0; i < MAX_TIMERS; i++) {
        timers[i].active = false;
        timers[i].growthFactor = 1.0f;
    }
}
```

### 6. Globals.h

```cpp
// REMOVE these lines:
constexpr float GROWING_MULTIPLIER = 1.5f;

// KEEP this line (still used as cap):
constexpr uint32_t MAX_GROWTH_INTERVAL_MS = MINUTES(120);
```

### 7. docs/sensor3_implementation_guide.md

```cpp
// BEFORE
TimerManager::instance().create(1000, -10, cb_sensor3Init);  // 10 exp retries

// AFTER
TimerManager::instance().create(1000, 10, cb_sensor3Init, 1.5f);  // 10 retries with 1.5x growth
```

## Caller Impact

**Code using negative repeat (must update):**

| # | File | Line | Current | New |
|---|------|------|---------|-----|
| 1 | lib/Globals/I2CInitHelper.h | 27 | `int maxRetries` | `uint8_t maxRetries; float growth` |
| 2 | lib/Globals/I2CInitHelper.cpp | 63 | passes cfg.maxRetries to create() | pass cfg.maxRetries, cfg.growth |
| 3 | lib/SensorManager/SensorManager.cpp | 142 | `-14, 500` | `14, 500, 1.5f` |
| 4 | lib/SensorManager/SensorManager.cpp | 156 | `-13, 1000` | `13, 1000, 1.5f` |
| 5 | lib/ConductManager/Clock/ClockPolicy.cpp | 65 | `-10, 1000` | `10, 1000, 1.5f` |
| 6 | lib/WiFiManager/FetchManager.cpp | 331 | `create(1000, -14, cb_fetchNTP)` | `create(1000, 14, cb_fetchNTP, 1.5f)` |
| 7 | lib/WiFiManager/FetchManager.cpp | 332 | `create(2000, -14, cb_fetchWeather)` | `create(2000, 14, cb_fetchWeather, 1.5f)` |
| 8 | lib/WiFiManager/FetchManager.cpp | 349 | `create(100, -14, cb_fetchNTP)` | `create(100, 14, cb_fetchNTP, 1.5f)` |

**Documentation examples (must update):**
- lib/TimerManager/README.md — examples
- docs/sensor3_implementation_guide.md — example
- docs/plan_i2c_init_refactor.md — examples

All other callers use positive repeat values or 0 (infinite) → no change needed, default `growth = 1.0f`.

## Migration Checklist

1. [ ] Update TimerManager.h struct and signatures
2. [ ] Update TimerManager.cpp implementation
3. [ ] Update TimerManager README.md examples
4. [ ] Remove GROWING_MULTIPLIER from Globals.h
5. [ ] Update I2CInitHelper.h struct (add growth field)
6. [ ] Update I2CInitHelper.cpp (pass growth to create())
7. [ ] Update SensorManager.cpp (2 calls: distance, lux)
8. [ ] Update ClockPolicy.cpp (1 call: RTC)
9. [ ] Update FetchManager.cpp (3 calls: NTP, weather)
10. [ ] Update sensor3_implementation_guide.md example
11. [ ] Update plan_i2c_init_refactor.md examples
12. [ ] Bump FIRMWARE_VERSION
13. [ ] Compile and test

## Backward Compatibility

None needed - updating all callers:
- 3 via I2CInitHelper (Distance, Lux, RTC)
- 3 direct FetchManager calls (NTP, Weather)
- Documentation examples
