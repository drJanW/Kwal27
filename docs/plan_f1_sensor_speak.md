# Plan F1: Sensor + Speak Architecture Completion

**Datum:** 2025-12-27  
**Status:** PLAN - wacht op APPROVED

## Probleem

1. Sensor orchestratie zit in `SensorManager.cpp` (Manager layer)
2. Zou in `SensorDirector` moeten (Conduct layer) per architectuur
3. Speak module is niet gekoppeld aan sensor events
4. SensorsConduct/Policy/Director zijn grotendeels stubs

## Huidige Structuur

```
SensorManager.cpp
  ├── cb_distanceSensorInit()     // retry logic, NotifyState calls
  ├── cb_luxSensorInit()          // retry logic, NotifyState calls  
  └── cb_readDistance/Lux()       // periodic reads

SensorsBoot.cpp
  └── plan() → arms init timers   // OK

SensorsConduct.cpp
  └── stub                        // LEEG

SensorsPolicy.cpp
  └── stub                        // LEEG

SensorDirector.cpp
  └── TODO placeholder            // LEEG
```

## Gewenste Structuur

```
SensorManager.cpp
  └── hardware access only (begin, read, isReady)

SensorsBoot.cpp
  └── plan() → SensorDirector::armInitTimers()

SensorDirector.cpp
  ├── armInitTimers()             // creates retry timers
  ├── cb_distanceInit()           // calls SensorManager::beginDistance()
  ├── cb_luxInit()                // calls SensorManager::beginLux()
  └── cb_sensor3Init()            // calls SensorManager::beginSensor3()

SensorsConduct.cpp
  ├── onDistanceReading(mm)       // called by periodic timer
  ├── onLuxReading(lux)           // called by periodic timer
  └── triggerSpeak(SpeakIntent)   // route to SpeakConduct

SensorsPolicy.cpp
  ├── shouldSpeak(event)          // throttle logic
  ├── distanceThreshold()         // from Globals
  └── luxThreshold()              // from Globals
```

## Speak Integration

### Aanpak

1. **TTS primair** - wacht tot `NotifyState::isOk(COMP_TTS)`, dan `AudioPolicy::requestSentence()`
2. **MP3 fallback** - alleen als TTS niet beschikbaar (boot fase)

### Zinnen (TTS)

| Event | Zin |
|-------|-----|
| Distance sensor fail | "Afstandmeter werkt niet" |
| Lux sensor fail | "Lichtmeting werkt niet" |
| Sensor3 fail | "Sensor3 ontbreekt" |
| Distance cleared (na ping) | "Object is verdwenen" |

### Flow

```
cb_distanceInit() last retry failed
  → SensorsConduct::onSensorFail(COMP_DIST)
    → if (NotifyState::isOk(COMP_TTS))
        AudioPolicy::requestSentence("Afstandmeter werkt niet")
      else
        // MP3 fallback of skip

cb_readDistance() [TimerManager]
  → distance was < min, now >= min (ping ended)
    → SensorsConduct::onDistanceCleared()
      → if (NotifyState::isOk(COMP_TTS))
          AudioPolicy::requestSentence("Object is verdwenen")
```

## Pseudocode

### SensorDirector.cpp

```cpp
namespace {
    void cb_distanceInit() {
        // NO GUARD - timer is al gecanceld als sensor ready
        
        int remaining = TimerManager::instance().getRepeatCount(cb_distanceInit);
        if (remaining != -1)
            NotifyState::set(COMP_DIST, abs(remaining));
        
        if (SensorManager::beginDistance()) {
            TimerManager::instance().cancel(cb_distanceInit);
            SensorsConduct::onSensorReady(COMP_DIST);  // Via eigen Conduct
            return;
        }
        
        if (abs(remaining) == 1) {
            SensorsConduct::onSensorFail(COMP_DIST);  // Via eigen Conduct
        }
    }
    
    void cb_luxInit() {
        // same pattern as distance
    }
}

void SensorDirector::armInitTimers() {
    auto& TM = TimerManager::instance();
    TM.restart(DISTANCE_INIT_INTERVAL, DISTANCE_MAX_RETRIES, cb_distanceInit);
    TM.restart(LUX_INIT_INTERVAL, LUX_MAX_RETRIES, cb_luxInit);
}
```

### SensorsConduct.cpp

```cpp
void SensorsConduct::onSensorReady(StatusComponent comp) {
    NotifyState::setOk(comp, true);
    
    switch (comp) {
        case COMP_DIST:
            NotifyConduct::report(NotifyIntent::DISTANCE_SENSOR_OK);
            break;
        case COMP_LUX:
            NotifyConduct::report(NotifyIntent::LUX_SENSOR_OK);
            break;
        default:
            break;
    }
}

void SensorsConduct::onSensorFail(StatusComponent comp) {
    NotifyState::setOk(comp, false);
    
    // Report to NotifyConduct
    switch (comp) {
        case COMP_DIST:
            NotifyConduct::report(NotifyIntent::DISTANCE_SENSOR_FAIL);
            break;
        case COMP_LUX:
            NotifyConduct::report(NotifyIntent::LUX_SENSOR_FAIL);
            break;
        default:
            break;
    }
    
    // TTS announcement (if ready)
    if (!NotifyState::isOk(COMP_TTS)) return;
    
    switch (comp) {
        case COMP_DIST:
            AudioPolicy::requestSentence("Afstandmeter werkt niet");
            break;
        case COMP_LUX:
            AudioPolicy::requestSentence("Lichtmeting werkt niet");
            break;
        case COMP_SENSOR3:
            AudioPolicy::requestSentence("Sensor3 ontbreekt");
            break;
        default:
            break;
    }
}

void SensorsConduct::onDistanceCleared() {
    if (!NotifyState::isOk(COMP_TTS)) return;
    AudioPolicy::requestSentence("Object is verdwenen");
}
```

### SensorsPolicy.cpp

```cpp
namespace {
    uint32_t lastDistanceClearedSpeak = 0;
    constexpr uint32_t SPEAK_COOLDOWN_MS = 30000;  // 30 sec
}

bool SensorsPolicy::canSpeakDistanceCleared() {
    uint32_t now = ClockService::instance().uptimeMs();
    
    if (now - lastDistanceClearedSpeak < SPEAK_COOLDOWN_MS)
        return false;
    
    lastDistanceClearedSpeak = now;
    return true;
}
```

## Bestanden te Wijzigen

| # | File | Wijziging |
|---|------|-----------|
| 1 | `lib/SensorManager/SensorManager.cpp` | Remove init logic, keep hardware only |
| 2 | `lib/SensorManager/SensorManager.h` | Add beginDistance(), beginLux(), isXxxReady() |
| 3 | `lib/ConductManager/Sensors/SensorDirector.cpp` | Init timers + callbacks |
| 4 | `lib/ConductManager/Sensors/SensorDirector.h` | armInitTimers() |
| 5 | `lib/ConductManager/Sensors/SensorsConduct.cpp` | onDistanceReading, onLuxReading |
| 6 | `lib/ConductManager/Sensors/SensorsConduct.h` | public methods |
| 7 | `lib/ConductManager/Sensors/SensorsPolicy.cpp` | canSpeakDistanceCleared, cooldown |
| 8 | `lib/ConductManager/Sensors/SensorsPolicy.h` | public methods |
| 9 | `lib/ConductManager/Sensors/SensorsBoot.cpp` | call SensorDirector::armInitTimers() |

## Verificatie

1. Boot: sensors init via Director callbacks
2. Serial: `[SensorDirector] Distance init OK` ipv `[SensorManager]`
3. Sensor fail + TTS ready → hear "Afstandmeter werkt niet" etc
4. Object weg na ping → hear "Object is verdwenen"
5. Cooldown: no repeat speak within 30 sec

## Dependencies

- Geen nieuwe libraries
- Geen nieuwe timers (hergebruik bestaande)
- Speak module al aanwezig

## Risico's

- SensorManager refactor raakt veel code
- Speak MP3 files moeten bestaan voor nieuwe intents

---

**APPROVED?**
