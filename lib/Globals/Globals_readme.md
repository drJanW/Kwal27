# Globals — Global Constants, Config Parameters & Utilities

**Role in Kwal27:** Foundation layer shared by all other modules. Provides compile-time constants, runtime configuration parameters (`Globals` struct), hardware pin definitions, logging macros, utility functions, and the firmware version code. All modules `#include "Globals.h"`.

## Files

### Globals.h / Globals.cpp
The central configuration header. Contains:
- `FIRMWARE_VERSION_CODE` — single authoritative version string (e.g. `"260614B"`)
- Timing constant macros: `SECONDS(x)`, `MINUTES(x)`, `HOURS(x)`
- Capacity defines: `MAX_THEME_DIRS` (500), `MAX_TIMERS` (50)
- `hwStatus` — `uint16_t` hardware availability register (bits set during boot for each available peripheral)
- `struct Globals` — runtime-overridable parameters loaded from `globals.csv` on SD card. Organized in groups:
  - **Audio** (20 params): min/max intervals, volume boundaries, fade times, TTS cache settings, demo config
  - **Speech** (2 params): saytime/temperature intervals, web expiry
  - **Light/Pattern** (5 params): fallback intervals, shift check, fade width, color/pattern change intervals
  - **Brightness/Lux** (10 params): min/max brightness, brightness curve segments, ambient light calibration
  - **Time** (2 params): NTP sync interval, clock tick interval
  - **WiFi** (2 params): connection timeout, AP fallback timeout
  - **System** (4 params): deep sleep delay, heap warning threshold, log buffer size

All fields are `inline static` with sensible defaults, overridable from CSV.

### HWconfig.h
Hardware pin assignments for the specific Kwal27 PCB. Defines GPIO numbers for: I2C bus (SDA/SCL), SD card CS, LED data pin, audio I2S pins (BCLK, LRC, DOUT), distance sensor shutdown/enable, RTC interrupt, and WiFi status LED.

### MathUtils.h
Single-header utility math functions. Provides `mapFloat()` (floating-point Arduino `map`), `lerpFloat()`, clamping, and angle normalization helpers used by light rendering and sensor processing.

### AudioState.h / AudioState.cpp
Audio status flag manager. Tracks audio subsystem state (idle, playing fragment, playing sentence, playing PCM, stopping). Uses `std::atomic` flags for thread-safe status checks across timer callbacks and web handlers.

### CsvUtils.h / CsvUtils.cpp
General-purpose CSV file parsing utilities. Handles semicolon-delimited files (the Kwal standard format), `#` comment line skipping, quoted-field support, and whitespace trimming. Shared by all CSV catalog loaders (calendar, globals, light_colors, light_patterns, theme_boxes, shifts).

### I2CInitHelper.h / I2CInitHelper.cpp
One-shot I2C bus initialization helper. Detects I2C hardware presence, handles bus recovery, and sets the `hwStatus` bit if the bus initializes successfully. Called once at boot.

### LogBuffer.h / LogBuffer.cpp
Circular RAM buffer for diagnostic log messages. Stores recent log lines (size configurable via Globals). Exposed via web API (`/api/log`) for remote debugging without serial access. Uses `PL()` / `PF()` / `LOG_DEBUG()` / `LOG_WARN()` macros defined in `macros.inc`.

### macros.inc
Preprocessor macro definitions included by `Globals.h`. Defines:
- `PL(...)`, `PF(...)` — Serial print/log macros
- `LOG_DEBUG(...)`, `LOG_WARN(...)` — Conditional log macros
- `LOG_BOOT_SPAM` — Compile-time flag for verbose boot logging
- Helper macros for min/max/constrain