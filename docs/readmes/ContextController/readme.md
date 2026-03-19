# ContextController

> Version: 260319A | Updated: 2026-03-19

The runtime brain of the system: gathers state from the environment, normalizes it, and surfaces "what is happening now" to the rest of the system. Timer-driven updates and sensor snapshots all feed into shared context.

## Files (30)

### Core
| File | Version | Purpose |
|------|---------|---------|
| `ContextController.h/.cpp` | 260227B | Central coordination: time state, weather, clock, NTP |
| `ContextManager.cpp` | — | Lifecycle management (implementation-only, no .h) |

### Calendar
| File | Version | Purpose |
|------|---------|---------|
| `Calendar.h/.cpp` | 260216H | Calendar day structure and loading from CSV |
| `CalendarCsv.h/.cpp` | 260202A | CSV parser for calendar data |
| `CalendarREADME.md` | — | Calendar subsystem notes (stub) |

### Context State
| File | Version | Purpose |
|------|---------|---------|
| `ContextFlags.h/.cpp` | 260202A | Context flag management |
| `ContextStatus.h` | 260202A | Status type definitions |
| `StatusBits.h` | 260205A | Enum `TimeStatus` bit positions |
| `StatusFlags.h/.cpp` | 260215B | Hardware fail bits, time-of-day/season/weather/moon/temperature status |

### Catalog Loaders
| File | Version | Purpose |
|------|---------|---------|
| `LightColorCatalog.h/.cpp` | 260204A | Color palette loader from `light_colors.csv` |
| `LightColors.h/.cpp` | 260202A | Wrapper: active color selection |
| `LightPatternCatalog.h/.cpp` | 260204A | Pattern loader from `light_patterns.csv` |
| `LightPatterns.h/.cpp` | 260202A | Wrapper: active pattern selection |
| `ThemeBoxTable.h/.cpp` | 260204A | Theme box loader from `theme_boxes.csv` |
| `ThemeBoxManager.cpp` | — | Theme box lifecycle (implementation-only, no .h) |

### Time and State
| File | Version | Purpose |
|------|---------|---------|
| `TimeOfDay.h/.cpp` | 260202A | Time-of-day period detection (dawn, day, dusk, night, etc.) |
| `TodayContext.h/.cpp` | 260213A | Wrapper for TodayState |
| `TodayModels.h` | 260202A | Structs: ThemeBox, LightPattern, RgbColor, LightColor, TodayState |
| `TodayState.h` | 260202A | Daily state init and query interface |

## API

### ContextController (namespace)
```cpp
void begin();
const TimeState& time();
void refreshTimeRead();

// Clock
void setClockTime(uint8_t h, uint8_t m, uint8_t s);
void setClockDate(uint8_t d, uint8_t m, uint16_t y);
void setClockDayCalc(uint8_t dow, uint16_t doy);
void setSunrise(uint8_t h, uint8_t m);
void setSunset(uint8_t h, uint8_t m);
void setTimeSynced(bool) / bool isTimeSynced();

// Weather
void updateWeather(...);
void clearWeather();
void updateRtcTemperature(...);
void clearRtcTemperature();

// Moon
void computeMoonPhase();

// NTP
void setNtpResyncCallback(ResyncCallback cb);
void requestNtpResync();

// Web integration
bool ContextController_post(WebCmd cmd, uint8_t dir, uint8_t file, int8_t delta);
```

### Calendar
```cpp
class CalendarSelector {
    bool begin(fs::FS& sd, const char* rootPath);
    bool loadToday(uint16_t year, uint8_t month, uint8_t day);
    const CalendarData& calendarData() const;
    bool hasCalendarData() const;
    bool isReady() const;
};
```

### Catalogs
```cpp
// LightColorCatalog / LightPatternCatalog / ThemeBoxTable (same pattern)
bool begin(fs::FS& sd, const char* rootPath);
const T* find(uint8_t id) const;
const T* active() const;
```

### TimeOfDay
```cpp
uint64_t getActiveStatusBits();
bool isNight() / isDawn() / isMorning() / isLight() / isDay();
bool isAfternoon() / isDusk() / isEvening() / isDark();
bool isAM() / isPM();
```

### StatusFlags
```cpp
uint64_t getFullStatusBits();
uint64_t getTimeOfDayBits() / getSeasonBits() / getWeekdayBits();
uint64_t getWeatherBits() / getMoonPhaseBits();
uint64_t getHardwareFailBits() / getTemperatureShiftBits();
float getTemperatureSwing();
```

### TodayState
```cpp
bool InitTodayState(fs::FS& sd, const char* rootPath);
bool TodayStateReady();
bool LoadTodayState(TodayState& state);
const ThemeBox* FindThemeBox(uint8_t id);
const ThemeBox* GetDefaultThemeBox();
const std::vector<ThemeBox>& GetAllThemeBoxes();
```

## Architecture

```
Calendar CSV --> CalendarSelector --> active theme box
                                         |
LightColors CSV --> LightColorCatalog ---+--> TodayState
LightPatterns CSV -> LightPatternCatalog |        |
ThemeBoxes CSV --> ThemeBoxTable ---------+        v
                                          RunManager/Calendar/
Clock/Sensors --> TimeOfDay                RunManager/Light/
              --> StatusFlags              RunManager/Audio/
```

All consumers read from ContextController; it never calls into Run or Controller layers.