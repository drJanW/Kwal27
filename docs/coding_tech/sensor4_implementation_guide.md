# Sensor4 Implementation Guide

> Version: 260319A | Updated: 2026-03-19
>
> **Basis:** Follow [sensor3_implementation_guide.md](sensor3_implementation_guide.md) — this document describes only the **extra** steps.

## Sensor4 does not exist yet

Sensor3 has stubs and placeholders. Sensor4 must be created from scratch.

## Files to add/modify

### 1. Globals.h — add presence flag

```cpp
inline static bool sensor4Present = false;  // placeholder
```

### 2. Globals.h / Globals.cpp — add dummy value

```cpp
// Globals.h
inline static float sensor4DummyTemp = 25.0f;

// Globals.cpp — in loadFromCSV:
PARSE_FLOAT("sensor4DummyTemp", sensor4DummyTemp)
PARSE_BOOL("sensor4Present", sensor4Present)
```

### 3. globals.csv

```csv
#sensor4Present;b;false;activate sensor4 hardware
#sensor4DummyTemp;f;25.0;fallback when sensor4 absent
```

### 4. AlertRequest.h

```cpp
SENSOR4_OK,
SENSOR4_FAIL,
```

### 5. AlertState.h / AlertState.cpp

```cpp
// AlertState.h — in StatusComponent enum
SC_SENSOR4,   // after SC_SENSOR3
SC_COUNT = 13 // bump count

// AlertState.cpp — add legacy convenience functions
bool isSensor4Ok();
void setSensor4Status(bool);
```

Update `isPresent()` to check `Globals::sensor4Present`.

### 6. AlertRun.cpp

```cpp
case AlertRequest::SENSOR4_OK:
    AlertState::setSensor4Status(true);
    break;
case AlertRequest::SENSOR4_FAIL:
    AlertState::setSensor4Status(false);
    break;
```

### 7. AlertRGB.cpp

Check `Globals::sensor4Present` before flashing (same pattern as sensor3).

### 8. SpeakRequest.h

```cpp
SENSOR4_FAIL,
```

### 9. SpeakRun.cpp

```cpp
// In getTtsSentence()
case SpeakRequest::SENSOR4_FAIL:
    return "Sensor vier ontbreekt.";

// In speakFailures()
if (!AlertState::isSensor4Ok() && Globals::sensor4Present)
    speak(SpeakRequest::SENSOR4_FAIL);
```

### 10. SensorController.cpp

```cpp
bool sensor4Ready = false;
bool sensor4InitFailed = false;

void cb_sensor4Init();
void beginSensor4();
bool isSensor4Ready();
float getSensor4Value();
```

Follow exact same pattern as sensor3 (see sensor3 guide section 3-4).

### 11. SensorsBoot.cpp

```cpp
if (Globals::sensor4Present)
    SensorController::beginSensor4();
```

### 12. WebGUI health.js

```javascript
// In health bit mapping
{ comp: SC_SENSOR4, name: "Sensor4", icon: "..." }
```

## Checklist

- [ ] Globals.h presence flag + dummy value
- [ ] Globals.cpp CSV parsing
- [ ] globals.csv entries (commented out)
- [ ] AlertRequest enum (OK + FAIL)
- [ ] SC_SENSOR4 in StatusComponent enum
- [ ] AlertState set/is functions
- [ ] AlertRun case handlers
- [ ] AlertRGB flash guard
- [ ] SpeakRequest enum
- [ ] SpeakRun TTS + speakFailures
- [ ] SensorController init + read
- [ ] SensorsBoot guard + call
- [ ] WebGUI health mapping
