# ContextController — Context & State Aggregation Module

**Role in Kwal27:** The "brain" that reads and distributes all system context — time, calendar, light colors/patterns, theme boxes, sensors, status flags, and web commands. Other modules consume context via this module rather than directly depending on each other.

## Files

### ContextController.h / ContextController.cpp
Public API namespace for time, sensor, and context state. Exposes `TimeState` struct (full snapshot of PRTClock + weather + RTC temp) via `time()` and `refreshTimeRead()`. Provides clock-write API (`setClockTime`, `setClockDate`, `setClockDayCalc`, `setSunrise`, `setSunset`) — decouples modules that need to set time from ClockController. Also handles NTP resync callbacks and web commands (`WebCmd` enum: NextTrack, DeleteFile, ApplyVote, BanFile) via `ContextController_post()`.

### ContextManager.cpp (no .h)
Private implementation of the public ContextController API. Manages the periodic state tick timer and connects ContextController to other subsystems (PRTClock, sensors, web loop).

### Calendar.h / Calendar.cpp
Calendar file parser and selector. `CalendarSelector` loads `calendar.csv`, which maps dates to theme boxes, patterns, colors, and TTS sentences. Structures: `CalendarEntry` (one day), `CalendarThemeBox` (theme box definition), `CalendarData` (day + theme combined), `NextEventInfo` (lookahead for upcoming events). API: `begin()`, `loadToday()`, `findNextEntry()`.

### CalendarCsv.h / CalendarCsv.cpp
Low-level CSV parser for `calendar.csv`. Handles semicolon-delimited parsing, `#` comment lines, and populates `CalendarEntry` structs. `CalendarREADME.md` in this directory documents the CSV column layout.

### ContextFlags.h / ContextFlags.cpp
Thin wrapper that re-exports `StatusFlags.h`. Provides the context-level status flag API used throughout the system.

### StatusFlags.h / StatusFlags.cpp
Bitmask-based status flag manager. Tracks system states (boot phase, SD ready, WiFi connected, audio playing, etc.) as a `uint64_t` bitmask. Used by policies and directors to gate behavior based on system state.

### StatusBits.h
Enumeration of all status bit positions in the `uint64_t` status mask. Defines named constants for each system state flag.

### LightColorCatalog.h / LightColorCatalog.cpp
Runtime catalog of named color palettes loaded from `light_colors.csv`. Each entry has an ID, color name, two CRGB values (RGB1/RGB2), and pattern parameters. API: `begin()`, `find()`, `count()`.

### LightColors.h / LightColors.cpp
Deprecated thin wrapper — re-exports `LightColorCatalog.h`. Retained for legacy compatibility.

### LightPatternCatalog.h / LightPatternCatalog.cpp
Runtime catalog of light patterns loaded from `light_patterns.csv`. Each entry has an ID, pattern name, and parameters that fill `LightShowParams`. API: `begin()`, `find()`, `findByName()`.

### LightPatterns.h / LightPatterns.cpp
Deprecated thin wrapper — re-exports `LightPatternCatalog.h`. Retained for legacy compatibility.

### ThemeBoxTable.h / ThemeBoxTable.cpp
Loads and indexes `theme_boxes.csv`, which maps theme box IDs to lists of SD card directories containing audio files. API: `begin()`, `find()` by ID, `active()` for the currently selected theme box, `boxes()` for full iteration.

### ThemeBoxManager.cpp (no .h)
Manages theme box selection logic: picks a theme box based on calendar entries, Lux calibration state, or TV simulator mode. Not a class — namespace-level functions that operate on `ThemeBoxTable`.

### TimeOfDay.h / TimeOfDay.cpp
Time-of-day period detection driven by sunrise/sunset times from PRTClock. Returns `uint64_t` bitmask of active periods (night, dawn, morning, light, day, afternoon, dusk, evening, dark, AM, PM). Used by policies to adjust brightness, patterns, and audio intervals.

### TodayContext.h / TodayContext.cpp
Aggregates all "today" state: calendar entry, theme box, light pattern, light color, time-of-day flags, and sensor data. Provides a single snapshot for policy decisions. `TodayState` struct is the consolidated context consumed by Run policies.

### TodayModels.h
Shared data structures used by TodayContext and ThemeBoxTable: `ThemeBoxDirectories` (directory lists), `ThemeBox` (theme box with audio dirs), `AudioScoreWeights` (per-directory weight multipliers).

### TodayState.h
Defines the `TodayState` struct — the complete snapshot of today's context that policies evaluate. Includes current calendar, theme, light settings, time periods, and override flags.