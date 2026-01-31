# Today Context – Time-of-Day Bands

This note snapshot captures the phase scheme from the spreadsheet screenshot so
we do not lose it across sessions.

## Fixed Anchors

| Symbol       | Time  | Notes                         |
|--------------|-------|-------------------------------|
| `startday`   | 07:00 | Earliest daytime request start |
| `endday`     | 18:00 | Latest daytime request end     |
| `endevening` | 23:00 | Hard cutoff for evening band  |

`startmorning = max(sunrise, startday)`  and
`endafternoon = min(sunset, endday)`.

## Derived Boundaries

| Band          | From           | To             |
|---------------|----------------|----------------|
| `isNight`     | midnight       | `startmorning` |
| `isDawn`      | `sunrise - 1h` | `sunrise + 1h` |
| `isLight`     | `sunrise + 1h` | `sunset - 1h`  |
| `isMorning`   | `startmorning` | noon           |
| `isDay`       | `startday`     | `endday`       |
| `isAM`        | 00:00          | 11:59          |
| `isPM`        | 12:00          | 23:59          |
| `isAfternoon` | noon           | `endday`       |
| `isDusk`      | `sunset - 1h`  | `sunset + 1h`  |
| `isEvening`   | `endafternoon` | midnight       |
| `isDark`      | `sunset + 1h`  | `sunrise + 1h` |

Minutes are kept inclusive of the left boundary and exclusive of the right.

## Evaluation Guidance

* Always compute `startmorning` and `endafternoon` after sunrise/sunset are
  known for the day. When the solar data is missing, fall back to the fixed
  anchors above.
* Evaluate periods every 10 minutes; crossing guards decide when each boolean
  flips.
* Each band lives on TodayContext and is consumed by policy layers.
* Keep terminology stable (`isMorning`, `isDusk`, etc.) so it matches the
  spreadsheet and charter clause 2.2.

## Dynamic Adjustment Principle

External factors (such as time-of-day, sensor input, or user overrides) act as modifiers to the system's parameters, rather than setting absolute values. These modifiers apply shifts relative to the current/actual parameter values, allowing the environment to adapt in real-time to context changes (e.g., morning, evening, special events).

Key points:
- External factors act as modifiers, not absolute setters.
- Shifts are applied relative to the current/actual parameter values, preserving the base state and allowing for cumulative or reversible changes.
- This principle can be extended to brightness, patterns, audio volume, etc., for a unified adjustment system.
