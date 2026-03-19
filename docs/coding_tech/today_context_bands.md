# Today Context - Time-of-Day Bands

> Version: 260319A | Updated: 2026-03-19

## Time-of-Day Constants (TimeOfDay.cpp)

Namespace-scoped constants in minutes since midnight:

| Symbol | Time | Minutes | Notes |
|--------|------|---------|-------|
| `dawnStart` | 05:00 | 300 | Start of dawn period |
| `morningStart` | 07:00 | 420 | Start of morning |
| `dayStart` | 09:00 | 540 | Start of full day |
| `afternoonStart` | 12:00 | 720 | Start of afternoon |
| `duskStart` | 17:00 | 1020 | Start of dusk period |
| `eveningStart` | 19:00 | 1140 | Start of evening |
| `nightStart` | 22:00 | 1320 | Start of night |
| `fallbackSunrise` | 07:00 | 420 | Used when real sunrise unknown |
| `fallbackSunset` | 19:00 | 1140 | Used when real sunset unknown |

## Boolean Functions (TimeOfDay.h)

Bands use a mix of fixed constants and real sunrise/sunset data.

### Fixed-boundary bands

| Function | From | To | Notes |
|----------|------|----|-------|
| `isNight()` | nightStart (22:00) | dawnStart (05:00) | Wraps midnight |
| `isMorning()` | morningStart (07:00) | afternoonStart (12:00) | |
| `isDay()` | dayStart (09:00) | duskStart (17:00) | |
| `isAfternoon()` | afternoonStart (12:00) | duskStart (17:00) | |
| `isEvening()` | eveningStart (19:00) | nightStart (22:00) | |
| `isAM()` | 00:00 | 11:59 | Before noon |
| `isPM()` | 12:00 | 23:59 | From noon onward |

### Sunrise/sunset-dependent bands

| Function | From | To | Fallback |
|----------|------|----|----------|
| `isDawn()` | sunrise - 1h | sunrise | fallbackSunrise |
| `isLight()` | sunrise | sunset | fallbackSunrise/Sunset |
| `isDusk()` | sunset | sunset + 1h | fallbackSunset |
| `isDark()` | NOT isLight() | | |

### Status bits

`getActiveStatusBits()` returns a `uint64_t` bitmask of all currently active
time-of-day flags. Used by shift tables to look up time-dependent adjustments.

## Band Overlap

Bands are NOT mutually exclusive. Multiple bands can be active simultaneously:

```
05:00  06:00  07:00  09:00  12:00  17:00  19:00  22:00
  |      |      |      |      |      |      |      |
  |--dawn-|      |      |      |      |      |      |
  |      |------morning--------|      |      |      |
  |      |      |------day------------|      |      |
  |      |      |      |--afternoon---|      |      |
  |      |      |      |      |      |--evening-----|
night----|      |      |      |      |      |      |--night
         |------isLight (sunrise..sunset)---|      |
```

Example at 07:30 with sunrise at 06:30:
- `isMorning()` = true (07:00-12:00)
- `isLight()` = true (sunrise-sunset)
- `isDay()` = false (not yet 09:00)
- `isDawn()` = false (dawn ended at sunrise)

## Dynamic Adjustment Principle

External factors (time-of-day, sensor input, user overrides) act as modifiers
to parameter values via shift tables, not as absolute setters. Shifts are
applied relative to current values, preserving base state and allowing
cumulative or reversible changes.
