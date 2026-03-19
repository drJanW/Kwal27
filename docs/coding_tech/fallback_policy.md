# Fallback Policy

> Version: 260319A | Updated: 2026-03-19

Defines graceful degradation when hardware or modules fail.

## Hardware Status Layers

### hwStatus Bitmask (HWconfig.h)

Legacy runtime flags set during hardware init.

| Bit | Flag | Set When |
|-----|------|----------|
| 0 | `HW_SD` | SD card mounted |
| 1 | `HW_WIFI` | WiFi connected |
| 2 | `HW_AUDIO` | I2S init OK |
| 3 | `HW_RGB` | FastLED init OK |
| 4 | `HW_LUX` | VEML7700 detected |
| 5 | `HW_DIST` | VL53L1X detected |
| 6 | `HW_RTC` | DS3231 detected |
| 7 | `HW_I2C` | Wire.begin() OK |

`HW_ALL_CRITICAL = HW_SD | HW_AUDIO | HW_RGB`

### AlertState (lib/RunManager/Alert/AlertState.h)

Primary status system. Tracks per-component state with retry awareness.

```cpp
enum StatusComponent {
    SC_SD, SC_WIFI, SC_RTC, SC_AUDIO, SC_DIST, SC_LUX, SC_SENSOR3,
    SC_NTP, SC_WEATHER, SC_CALENDAR, SC_TTS, SC_NAS, SC_COUNT = 12
};

enum class SC_Status { OK, RETRY, LAST_TRY, FAILED, ABSENT };
```

**v4 API:**
- `uint8_t get(StatusComponent c)` — raw value
- `void set(StatusComponent c, T value)` — set from any type
- `SC_Status getStatus(StatusComponent c)` — typed status
- `bool isPresent(StatusComponent c)` — checks presence flag

**Legacy convenience API (backward compatible):**
- `isSdOk()`, `isWifiOk()`, `isRtcOk()`, `isNtpOk()`, etc.
- `setSdStatus(bool)`, `setWifiStatus(bool)`, etc.

**Gating functions:**
- `canPlayHeartbeat()` — always true
- `canPlayTTS()` — requires WiFi + Audio
- `canPlayMP3Words()` — requires SD + Audio
- `canPlayFragment()` — requires SD + Audio
- `canFetch()` — requires WiFi

**SD state:**
- `isSdBusy()` / `setSdBusy(bool)`
- `isSyncMode()` / `setSyncMode(bool)`
- `isIndexDirty()` / `setIndexDirty(bool)`

### Sensor Presence Flags (Globals.h)

Runtime `inline static bool` flags, CSV-overridable. NOT compile-time #defines.

| Flag | Default | Meaning |
|------|---------|---------|
| `Globals::luxSensorPresent` | true | VEML7700 expected |
| `Globals::distanceSensorPresent` | false | VL53L1X expected |
| `Globals::sensor3Present` | false | Board temp (placeholder) |
| `Globals::audioPresent` | true | I2S audio expected |
| `Globals::nasPresent` | true | NAS reachable expected |
| `Globals::rtcPresent` | true | DS3231 expected |

When a presence flag is false, AlertState marks the component as ABSENT:
no init attempt, no failure flash, no TTS announcement, WebGUI shows dash.

## SD Card Failure

**Trigger**: SD init fails after retries

**Behavior**:
- **Audio**: TTS only (no MP3 playback)
- **RGB**: Ambient preset — pink and turquoise slow crossfade
- **Calendar**: disabled, no shifts
- **Web UI**: returns "OUT OF ORDER" (HTTP 503)
- **Config**: flash defaults only

## No RTC Hardware

**Trigger**: DS3231 not detected on I2C

**Behavior**:
1. Try NTP sync
2. If NTP OK: use NTP time
3. If NTP fails: fallback date/time

**Fallback date** (HWconfig.h + Globals.h, CSV-overridable):
- Month: 4 (April)
- Day: 20
- Hour: 4 (04:00)
- Year: 2026

Rationale: 20 April has no holidays or special triggers. 04:00 is quiet night.

## No WiFi

**Trigger**: WiFi connect fails or disconnects

**Behavior**:
- **Time**: use RTC (if available)
- **NTP**: skip, rely on RTC
- **Weather**: defaults (temp: 15C, conditions: "unknown")
- **Sunrise/Sunset**: calculate locally using lat/lon from Globals (51.45, 5.45)
  - Calc error fallback: 07:00 / 19:00
- **Recovery**: background retry, fetch all on reconnect

## Sensor Failures

**Trigger**: Sensor init fails after retries with growing interval

**Behavior**:
- AlertRGB flash burst (state machine in AlertRGB.cpp)
- TTS announcement of failure
- Sensor returns dummy values

**Dummy values:**

| Sensor | Dummy Value | Rationale |
|--------|-------------|-----------|
| VL53L1X (distance) | 9999 mm | "far away" — no proximity triggers |
| VEML7700 (lux) | 0.5 (50%) | medium brightness |
| Sensor3 (board temp) | 25.0 C | placeholder — no hardware |

## AlertRGB Flash Patterns

State machine in AlertRGB.cpp handles flash sequences.

**API:**
- `startFlashing()` — start failure flash bursts
- `stopFlashing()` — stop and restore normal show
- `isFlashing()` — query if flashing active

**Timing (from Globals.h):**
- `flashBurstIntervalMs` = 10s between bursts
- `flashBurstRepeats` = 2 (3 total: initial + 2 repeats)
- `flashBurstGrowth` = 2.0 (exponential backoff)
- `reminderIntervalMs` = 2 min (long-term reminders)
- `reminderIntervalGrowth` = 10.0 (exponential)
- `flashCriticalMs` = 2s (critical alert duration)
- `flashNormalMs` = 1s (normal alert duration)

Checks `Globals::distanceSensorPresent`, `luxSensorPresent`, `sensor3Present`,
`nasPresent` — only flashes for sensors that are expected to be present.

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
- Vote delta clamped to +/-10 per request
- WebGuiStatus uses int16_t to prevent overflow in UI

## Implementation Locations

| Fallback | File |
|----------|------|
| LED map fallback (circular) | LEDMap.cpp |
| SD fail ambient show | LightController.cpp |
| Fallback date config | HWconfig.h + Globals.h |
| Sunrise calculation | FetchController.cpp |
| Sensor dummy returns | SensorController.cpp |
| "OUT OF ORDER" response | WebInterfaceController.cpp |
| Boot fragment trigger | CalendarRun.cpp / RunManager.cpp |
| Voting score | SDVoting.cpp, WebGuiStatus.cpp |
| Flash patterns | AlertRGB.cpp |
| Component status | AlertState.h |
