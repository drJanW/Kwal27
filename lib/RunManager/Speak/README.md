# Speak Module

> Version: 251230A | Updated: 2025-12-30

## Purpose
The Speak module is the Run layer for status and error audio feedback.
It routes speak intents to `PlaySentence` in AudioManager via modular word combinations.

## MP3 Word IDs
MP3 files are stored in `/000/` on the SD card. File naming: `000.mp3` through `100.mp3`.
Max files per directory: `SD_MAX_FILES_PER_SUBDIR = 101`

### ID Ranges (SpeakWords.h)

| Range | Usage | Files |
|-------|-------|-------|
| 0-59  | Spoken numbers (nul t/m negenenvijftig) | `000.mp3` - `059.mp3` |
| 60-72 | Subjects (onderwerpen) | `060.mp3` - `072.mp3` |
| 73-83 | States/actions (staten en acties) | `073.mp3` - `083.mp3` |
| 84-90 | Modifiers (bijwoorden) | `084.mp3` - `090.mp3` |
| 91-100| Time building blocks | `091.mp3` - `100.mp3` |
| 255   | END_OF_SENTENCE marker | (no file) |

### Subjects (60-72)

| Constant | ID | Audio |
|----------|----|---------|
| MP3_SYSTEM | 60 | "systeem" |
| MP3_WIFI | 61 | "wifi" |
| MP3_SENSOR | 62 | "sensor" |
| MP3_TEMPERATUUR | 63 | "temperatuur" |
| MP3_SD | 64 | "geheugenkaart" |
| MP3_OTA | 65 | "update" |
| MP3_WEB | 66 | "webinterface" |
| MP3_AUDIO | 67 | "audio" |
| MP3_LIGHT | 68 | "licht" |
| MP3_DISTANCE | 69 | "afstand" |
| MP3_CALENDAR | 70 | "kalender" |
| MP3_TIME | 71 | "tijd" |
| MP3_POWER | 72 | "voeding" |

### States/Actions (73-83)

| Constant | ID | Audio |
|----------|----|---------|
| MP3_START | 73 | "start" |
| MP3_GEREED | 74 | "gereed" |
| MP3_BEZIG | 75 | "bezig" |
| MP3_FOUT | 76 | "fout" |
| MP3_MISLUKT | 77 | "mislukt" |
| MP3_OPNIEUW | 78 | "opnieuw" |
| MP3_VERBINDING | 79 | "verbinding" |
| MP3_UPDATE | 80 | "update" |
| MP3_CONTROLE | 81 | "controle" |
| MP3_HERSTELD | 82 | "hersteld" |
| MP3_BESCHIKBAAR | 83 | "beschikbaar" |

### Modifiers (84-90)

| Constant | ID | Audio |
|----------|----|---------|
| MP3_GEEN | 84 | "geen" |
| MP3_LAGE | 85 | "lage" |
| MP3_HOGE | 86 | "hoge" |
| MP3_KRITIEK | 87 | "kritiek" |
| MP3_INFORMATIE | 88 | "informatie" |
| MP3_MELDING | 89 | "melding" |
| MP3_AUTOMATISCH | 90 | "automatisch" |

### Time Building Blocks (91-100)

| Constant | ID | Audio |
|----------|----|---------|
| MP3_HET_IS | 91 | "het is" |
| MP3_UUR | 92 | "uur" |
| MP3_MINUTEN | 93 | "minuten" |
| MP3_GRADEN | 94 | "graden" |
| MP3_PROCENT | 95 | "procent" |
| MP3_EN | 96 | "en" |
| MP3_GOEDEMORGEN | 97 | "goedemorgen" |
| MP3_GOEDEMIDDAG | 98 | "goedemiddag" |
| MP3_GOEDEAVOND | 99 | "goedeavond" |
| MP3_GOEDENACHT | 100 | "goedenacht" |

## Architecture
```
SpeakRun::speak(intent) → PlaySentence::start(words[])
SpeakRun::sayTime(h,m) → PlaySentence::start([HET_IS, h, UUR, m])
```

## Components

### SpeakWords.h
Defines `Mp3Id` enum with all MP3 file IDs. Modulair systeem: subjects + states + modifiers = flexible sentences.

### SpeakRun
- `plan()`: No timers to arm yet
- `speak(SpeakIntent)`: Routes intent to PlaySentence with word combination
- `speakFail(StatusComponent)`: Speaks failure for a component (lookup table)
- `sayTime(hour, minute)`: Says current time as sentence

### SpeakIntent enum
```cpp
enum class SpeakIntent : uint8_t {
    // Component failures (for boot notification)
    SD_FAIL,
    WIFI_FAIL,
    RTC_FAIL,
    NTP_FAIL,
    DISTANCE_SENSOR_FAIL,
    LUX_SENSOR_FAIL,
    SENSOR3_FAIL,
    WEATHER_FAIL,
    CALENDAR_FAIL,
    
    // Runtime events
    DISTANCE_CLEARED,
    
    // Special: say time (uses sentence)
    SAY_TIME,
    
    // Welcome greeting (time-based)
    WELCOME
};
```

## PlaySentence Implementation

### Word Queue (schuifregister)
- `s_wordQueue[20]` initialized with 255 (MP3_END)
- `playWord()` plays `wordQueue[0]`, creates timer for duration
- `cb_nextWord()` shifts queue left, calls `playWord()`
- Queue continues until `wordQueue[0] == 255`

### Functions
- `start(words[])`: Sets word queue and starts playback
- `stop()`: Cancels timer, clears queue, stops decoder

### Duration Estimation
`getMp3DurationMs()` estimates playback time per word based on syllables.
Plus WORD_EXTRA_FACTOR and WORD_INTERVAL_MS between words.

## Usage Example
```cpp
#include "SpeakRun.h"

// Speak a component failure:
SpeakRun::speak(SpeakIntent::WIFI_FAIL);      // "wifi mislukt"
SpeakRun::speak(SpeakIntent::RTC_FAIL);       // "tijd fout"

// Speak failure via StatusComponent:
SpeakRun::speakFail(SC_WIFI);                 // "wifi mislukt"

// Say the current time:
SpeakRun::sayTime(14, 30);                    // "het is veertien uur dertig"

// Welcome greeting (based on hour):
SpeakRun::speak(SpeakIntent::WELCOME);        // "goedemiddag"
```
