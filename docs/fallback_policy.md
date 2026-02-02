# Fallback Policy

Defines graceful degradation when hardware or modules fail.

**Version**: 260128A

## hwStatus Bits (HWconfig.h)

Runtime flags set during boot. Check with `if (hwStatus & HW_xxx)`.

| Bit | Flag | Set When | Missing → Trigger |
|-----|------|----------|-------------------|
| 0 | `HW_SD` | SD init OK | SD Card Failure |
| 1 | `HW_WIFI` | WiFi connected | No WiFi |
| 2 | `HW_AUDIO` | I2S init OK | (fatal) |
| 3 | `HW_RGB` | FastLED init OK | (fatal) |
| 4 | `HW_LUX` | VEML7700 detected | Sensor Failures |
| 5 | `HW_DIST` | VL53L1X detected | Sensor Failures |
| 6 | `HW_RTC` | DS3231 detected | No RTC Hardware |
| 7 | `HW_I2C` | Wire.begin() OK | All I2C sensors fail |

`HW_ALL_CRITICAL = HW_SD | HW_AUDIO | HW_RGB` — missing any triggers halt.

## SD Card Failure

**Trigger**: SD init fails after 10 retries

**Behavior**:
- **Audio**: TTS only (no MP3 playback)
- **RGB**: Ambient preset - pink ↔ turquoise fade
  - Pink: hue ~350°, high saturation
  - Turquoise/Aqua: hue ~180°, high saturation
  - Brightness varies 30%-100%
  - Slow crossfade between colors
- **Calendar**: disabled, no shifts
- **Web UI**: returns "OUT OF ORDER" (HTTP 503)
- **Config**: flash defaults only

## No RTC Hardware

**Trigger**: DS3231 not detected on I2C

**Behavior**:
1. Try NTP sync
2. If NTP OK → use NTP time
3. If NTP fails → fallback time: **20 april, 04:00**
   - 20 april: no holidays, no special triggers
   - 04:00: quiet night, minimal impact
   - Configurable via `Globals::fallbackMonth`, `fallbackDay`, `fallbackHour`

## No WiFi

**Trigger**: WiFi connect fails or disconnects

**Behavior**:
- **Time**: use RTC (if available)
- **NTP**: skip, rely on RTC
- **Weather**: use defaults
  - temp: 15°C
  - conditions: "unknown"
- **Sunrise/Sunset**: calculate locally
  - Uses lat/lon from Globals (default: 51.45, 5.45)
  - Simplified solar calculation
  - Calc error fallback: 07:00 / 19:00
- **Recovery**: background retry, fetch all on reconnect

## Sensor Failures

**Trigger**: Sensor init fails after retries with growing interval

**Behavior**:
- RGB alert flash burst (AlertRGB state machine)
- TTS announcement of failure
- Sensor returns dummy values

**Dummy values**:

| Sensor | Dummy Value | Rationale |
|--------|-------------|-----------|
| DistanceSensor (VL53L1X) | 9999 mm | "far away" - no proximity triggers |
| LuxSensor (VEML7700) | 0.5 (50%) | medium brightness |
| Sensor3 (board temp) | 25.0°C | placeholder - no hardware |

## Boot Fragment Timing

**Trigger**: CalendarRun sets theme box

**Behavior**:
- `RunManager::triggerBootFragment()` called after theme box ready
- One-shot flag prevents duplicate triggers
- 500ms delay before `cb_bootFragment`
- Retry if audio busy (TTS speaking sensor failures)

## Voting Score Storage

**Range**: 0-200 (uint8_t in SD index, int16_t in WebGuiStatus)

**Behavior**:
- Score 0 = banned file (excluded from selection)
- Vote delta clamped to ±10 per request
- WebGuiStatus uses int16_t to prevent overflow in UI

## Implementation Locations

| Fallback | File |
|----------|------|
| SD fail ambient show | LightController.cpp |
| Fallback date config | Globals.h/cpp (fallbackMonth/Day/Hour) |
| Sunrise calculation | FetchController.cpp |
| Sensor dummy returns | SensorController.cpp |
| "OUT OF ORDER" response | WebInterfaceController.cpp |
| Boot fragment trigger | CalendarRun.cpp → RunManager.cpp |
| Voting score | SDVoting.cpp, WebGuiStatus.cpp |

## AlertRGB Flash Patterns

State machine in AlertRGB.cpp handles flash sequences:
- Builds sequence from context bits (failures/successes)
- Single `cb_sequenceStep` callback advances through steps
- Colors: cyan (OK), magenta (warning), red (error), black (gap)

## AlertState Reports

Each failure should report via `AlertState::set*Status()`:

- `SD_FAIL` - after retries exhausted
- `WIFI_FAIL` - on disconnect (transition)
- `RTC_FAIL` - hardware not detected
- `NTP_FAIL` - timeout before fallback
- `DISTANCE_SENSOR_FAIL` / `LUX_SENSOR_FAIL` - init failed
- `TTS_FAIL` / `CALENDAR_FAIL` - subsystem errors

Success reports only when component actually works:
- `SD_OK`, `WIFI_OK`, `RTC_OK`, `NTP_OK`, `SENSOR*_OK`, `TTS_OK`, `CALENDAR_OK`
