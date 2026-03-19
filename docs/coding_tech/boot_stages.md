# Boot Stages Design

> Version: 260319A | Updated: 2026-03-19

## Architecture: BootSequencer + Cap

Boot uses a capability bitmask system. Each subsystem grants a Cap bit when
ready. Other subsystems declare which caps they need before they can run.

### Files

- `lib/RunManager/Boot/Cap.h` — Cap bitmask constants
- `lib/RunManager/Boot/BootSequencer.h/.cpp` — Sequencer logic
- `lib/RunManager/Boot/*.cpp` — Individual boot steps

## Cap Bitmask (Cap.h)

| Cap | Bit | Value | Meaning |
|-----|-----|-------|---------|
| `Cap::I2C` | 0 | 1 | I2C bus initialized |
| `Cap::RTC` | 1 | 2 | DS3231 RTC ready |
| `Cap::SD` | 2 | 4 | SD card mounted |
| `Cap::CONFIG` | 3 | 8 | globals.csv loaded |
| `Cap::WIFI` | 4 | 16 | WiFi connected |
| `Cap::WEB` | 5 | 32 | Web server started |
| `Cap::NTP` | 6 | 64 | NTP time synced |
| `Cap::CLOCK` | 7 | 128 | Clock running |
| `Cap::CSV` | 8 | 256 | CSV configs loaded |
| `Cap::SENSORS` | 9 | 512 | Sensors initialized |
| `Cap::SPEAK` | 10 | 1024 | TTS engine ready |
| `Cap::LIGHT_HW` | 11 | 2048 | FastLED initialized |
| `Cap::AUDIO_HW` | 12 | 4096 | I2S audio hardware ready |
| `Cap::CALENDAR` | 13 | 8192 | Calendar loaded, theme box set |

## BootSequencer API

```cpp
static void begin(uint16_t preGranted);  // Start boot with pre-granted caps
static void grant(uint16_t caps);        // Grant one or more caps
static void fail(uint16_t caps);         // Mark caps as failed
static bool has(uint16_t caps);          // Check if all caps in mask are granted
static bool isBootDone();                // All steps completed?
static uint16_t granted();               // Current granted bitmask
static const char* capName(uint16_t singleCap);  // Human-readable name
```

### StepResult Enum

Each boot step returns:
- `DONE` — step completed, grant its cap
- `PENDING` — not ready yet, try again later
- `FAILED` — step failed, mark cap as failed

### StepState Enum

Internal tracking per step:
- `WAITING` — prerequisites not yet met
- `RUNNING` — step executing
- `DONE` — completed successfully
- `FAILED` — gave up

## Boot Flow

```
begin(preGranted)
    |
    v
[For each step: check if required caps are granted]
    |
    +-- Requirements met? --> Run step
    |                           |
    |                           +-- DONE    --> grant(cap), advance
    |                           +-- PENDING --> retry later
    |                           +-- FAILED  --> fail(cap), skip
    |
    +-- Requirements NOT met? --> Wait
    |
    v
[All steps DONE or FAILED] --> isBootDone() = true
```

## hwStatus Bridge

`hwStatus` (HWconfig.h) still exists as a legacy bitmask set during hardware
init. RunManager reads hwStatus to compute `preGranted` caps for BootSequencer.

| hwStatus Bit | Flag | Cap Equivalent |
|-------------|------|----------------|
| 0 | `HW_SD` | `Cap::SD` |
| 1 | `HW_WIFI` | `Cap::WIFI` |
| 2 | `HW_AUDIO` | `Cap::AUDIO_HW` |
| 3 | `HW_RGB` | `Cap::LIGHT_HW` |
| 4 | `HW_LUX` | (sensor presence) |
| 5 | `HW_DIST` | (sensor presence) |
| 6 | `HW_RTC` | `Cap::RTC` |
| 7 | `HW_I2C` | `Cap::I2C` |

`HW_ALL_CRITICAL = HW_SD | HW_AUDIO | HW_RGB`

## Output Gating

Subsystems check caps before acting:

| Output | Required Caps | Example Check |
|--------|---------------|---------------|
| Heartbeat LED | none | Always available |
| RGB patterns | `Cap::SD + Cap::LIGHT_HW` | `BootSequencer::has(Cap::SD \| Cap::LIGHT_HW)` |
| TTS speech | `Cap::WIFI + Cap::AUDIO_HW + Cap::SPEAK` | `AlertState::canPlayTTS()` |
| MP3 playback | `Cap::SD + Cap::AUDIO_HW` | `AlertState::canPlayMP3Words()` |
| Fragment play | `Cap::SD + Cap::AUDIO_HW` | `AlertState::canPlayFragment()` |
| Web interface | `Cap::WIFI + Cap::WEB` | `BootSequencer::has(Cap::WEB)` |
| Network fetch | `Cap::WIFI` | `AlertState::canFetch()` |

## AlertState Integration

AlertState (lib/RunManager/Alert/AlertState.h) tracks per-component status:

```cpp
enum StatusComponent {
    SC_SD, SC_WIFI, SC_RTC, SC_AUDIO, SC_DIST, SC_LUX, SC_SENSOR3,
    SC_NTP, SC_WEATHER, SC_CALENDAR, SC_TTS, SC_NAS, SC_COUNT = 12
};

enum class SC_Status { OK, RETRY, LAST_TRY, FAILED, ABSENT };
```

AlertState provides `can*()` convenience functions that combine multiple
component checks for common gating decisions.
