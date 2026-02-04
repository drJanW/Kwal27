# Light Module

> Version: 251218A | Updated: 2025-12-17

Manages LED patterns and colors for the RGB ring display via `PatternCatalog` and `ColorsCatalog`.

## Architecture

```
┌─────────────────────────────────────────────────────────────────────────┐
│                              WebGUI                                      │
│  (dropdown selectie, sliders, save/delete knoppen)                      │
└─────────────────────────┬───────────────────────────────────────────────┘
                          │ HTTP GET/POST
                          ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                     WebInterfaceController                               │
│  /api/patterns  /api/colors  /api/patterns/select  etc.                 │
└─────────────────────────┬───────────────────────────────────────────────┘
                          │ LightRun::patternRead(), colorRead()
                          ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                        LightRun                                          │
│  - Source tracking (CONTEXT / MANUAL / CALENDAR)                        │
│  - Routes requests to catalogs                                           │
│  - Applies shifts from calendar                                         │
└─────────────────────────┬───────────────────────────────────────────────┘
                          │
          ┌───────────────┴───────────────┐
          ▼                               ▼
┌──────────────────────┐       ┌──────────────────────┐
│   PatternCatalog     │       │    ColorsCatalog     │
│  - patterns_ vector  │       │  - colors_ vector    │
│  - buildJson()       │       │  - buildColorsJson() │
│  - loadFromSD()      │       │  - loadFromSD()      │
│  - saveToSD()        │       │  - saveToSD()        │
└──────────┬───────────┘       └──────────┬───────────┘
           │                              │
           ▼                              ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                           SD Card                                       │
│  /light_patterns.csv                    /light_colors.csv               │
└─────────────────────────────────────────────────────────────────────────┘
```

## Data Flow: Patterns laden naar WebGUI dropdown

1. **Boot**: `PatternCatalog::begin()` → `loadFromSD()` → **random selection**
   - Leest `/light_patterns.csv` regel voor regel (streaming, niet hele file in geheugen)
   - Parset elke regel naar `PatternEntry` struct
   - Pusht naar `patterns_` vector (~100 bytes per entry)
   - **Selecteert random pattern uit geladen CSV** (sinds Dec 2025)
   - `ColorsCatalog::begin()` doet hetzelfde: random color bij boot

2. **WebGUI request**: `GET /api/patterns`
   - `WebInterfaceController::handlePatternsList()` 
   - → `LightRun::patternRead()` 
   - → `PatternCatalog::buildJson()`

3. **JSON bouwen**: `buildJson()` streamt direct naar String
   - Geen ArduinoJson DynamicJsonDocument (was 24-48KB buffer probleem!)
   - Output: ~250 bytes per pattern
   - Capaciteit: 100+ patterns zonder geheugenproblemen

4. **Browser**: JavaScript `Kwal.pattern.load()`
   - Parset JSON response
   - Vult `state.pattern.items` array
   - `updatePatternControls()` bouwt `<select>` dropdown

## Files

| File | Purpose |
|------|---------|
| `LightRun.cpp/h` | Request routing, source tracking |
| `LightPolicy.cpp/h` | Brightness/shift rules |
| `PatternCatalog.cpp/h` | Pattern CRUD, JSON streaming |
| `ColorsCatalog.cpp/h` | Color CRUD, JSON streaming |
| `ShiftTable.cpp/h` | Pattern/color shift percentages |
| `LightBoot.cpp/h` | Initialization |

## CSV Format

**light_patterns.csv** (16 columns, semicolon-delimited):
```
light_pattern_id;light_pattern_name;color_cycle_sec;bright_cycle_sec;fade_width;min_brightness;gradient_speed;center_x;center_y;radius;window_width;radius_osc;x_amp;y_amp;x_cycle_sec;y_cycle_sec
1;Smooth Orbit;18;14;99;12;0.6;0;0;22;24;4;3.5;2.5;22;20
```

**light_colors.csv** (4 columns):
```
light_colors_id;light_colors_name;rgb1_hex;rgb2_hex
1;Warm Sunset;#FF7F00;#552200
```

## Source Tracking

`LightRun` tracks where the active pattern/color came from:
- `CONTEXT` - Default from TodayState (no explicit selection)
- `MANUAL` - User selected via WebGUI
- `CALENDAR` - Set by calendar entry

This is included in JSON response as `"source"` field.

## Shift System

Pattern and color shifts adjust visual parameters based on time-of-day, season, weather, etc.

**Timer**: `cb_shiftTimer` fires every 60 seconds to check for context changes.

**Optimization**: Shifts only recompute when `StatusFlags::getFullStatusBits()` changes.
The 64-bit status word is cached in `s_lastStatusBits`. If unchanged, `applyToLights()` 
is skipped entirely - no multiplier calculations, no LED updates.

**Flow**:
1. `cb_shiftTimer()` → compare statusBits with cached value
2. If changed → `applyToLights()` recalculates all shifts and updates LEDs
3. If unchanged → reschedule timer, skip all computation

**Shift data**: Loaded from `/patternShifts.csv` and `/colorsShifts.csv` via `ShiftTable`.

## Limits

- Max 100 patterns (checked in WebGUI JS: `MAX_LIGHT_PATTERNS`)
- Max 100 colors
- Pattern entry: ~100 bytes in memory
- JSON output: ~250 bytes per pattern, ~80 bytes per color
