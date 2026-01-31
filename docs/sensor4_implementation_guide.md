# Sensor4 Implementation Guide

> **Basis:** Volg [sensor3_implementation_guide.md](sensor3_implementation_guide.md) - dit document beschrijft alleen de **extra** stappen.

## Sensor4 bestaat nog NIET

Sensor3 heeft al stubs/placeholders. Sensor4 moet volledig nieuw aangemaakt worden.

## Toe te voegen per file

### 1. HWconfig.h

```cpp
#define SENSOR4_FLASH_ENABLED false  // true wanneer hardware aanwezig
#define SENSOR4_DUMMY_TEMP 25.0f     // of andere default
```

### 2. Globals.h / Globals.cpp

```cpp
// Globals.h
extern float sensor4DummyTemp;

// Globals.cpp
float sensor4DummyTemp = 25.0f;

// In loadFromCSV:
PARSE_FLOAT("sensor4DummyTemp", sensor4DummyTemp)
```

### 3. globals.csv

```csv
#sensor4DummyTemp;f;25.0;fallback when sensor4 absent
```

### 4. AlertRequest.h

```cpp
SENSOR4_OK,
SENSOR4_FAIL,
```

### 5. AlertState.h / AlertState.cpp

```cpp
// AlertState.h - in Component enum
COMP_SENSOR4 = 7,  // bit 7 (na COMP_SENSOR3 = 6)

// AlertState.cpp
void setSensor4Status(bool isOK);
bool isSensor4Ok();

// Implementatie identiek aan setSensor3Status
```

### 6. AlertRun.cpp

```cpp
case AlertRequest::SENSOR4_OK:
    AlertState::setSensor4Status(true);
    break;
case AlertRequest::SENSOR4_FAIL:
    AlertState::setSensor4Status(false);
    break;
```

### 7. AlertRGB.cpp (optioneel)

```cpp
// Nieuwe kleur definiëren in AlertPolicy.h
static constexpr CRGB COLOR_SENSOR4 = CRGB::Cyan;  // kies kleur

// In AlertRGB.cpp flash functie
#if SENSOR4_FLASH_ENABLED
if (!AlertState::isSensor4Ok()) flashColor(COLOR_SENSOR4);
#endif
```

### 8. SpeakRequest.h

```cpp
SENSOR4_FAIL,
```

### 9. SpeakRun.cpp

```cpp
// In getTtsSentence()
case SpeakRequest::SENSOR4_FAIL:
    return "Sensor vier ontbreekt.";

// In getMp3Words()
case SpeakRequest::SENSOR4_FAIL:
    return {MP3_SENSOR, 4, MP3_END};

// In speakFailures()
if (!AlertState::isSensor4Ok()) speak(SpeakRequest::SENSOR4_FAIL);
```

### 10. SensorManager.cpp

```cpp
// Namespace variabelen
bool sensor4Ready = false;
bool sensor4InitFailed = false;

// Callback en begin functies (zie sensor3 guide)
void cb_sensor4Init();
void beginSensor4();
bool isSensor4Ready();
float getSensor4Value();
```

### 11. SensorsBoot.cpp

```cpp
SensorManager::beginSensor4();
```

### 12. WebGUI health.js

```javascript
// In parseHealth()
{ bit: 7, name: "Sensor4", icon: "🔧" },  // kies icon
```

## Checklist nieuwe sensor

- [ ] HWconfig.h defines
- [ ] Globals variabele + CSV
- [ ] AlertRequest enum (OK + FAIL)
- [ ] COMP_SENSOR4 in Component enum
- [ ] AlertState set/is functies
- [ ] AlertRun case handlers
- [ ] AlertRGB flash kleur (optioneel)
- [ ] SpeakRequest enum
- [ ] SpeakRun TTS + MP3 + speakFailures
- [ ] SensorManager init + read
- [ ] SensorsBoot aanroep
- [ ] WebGUI bit mapping
