# Plan B6: Brightness Slider Lux Trigger met Cooldown

**Datum:** 2025-12-27  
**Status:** PLAN - wacht op APPROVED

## Probleem

Na fix 251227E (verwijderen intentSetBrightness):
- Slider zet alleen `webBrightness` (correct)
- MAAR `baseBrightness` wordt niet herberekend met nieuwe webFactor
- Resultaat: slider heeft minder effect dan verwacht

## Root Cause

```
computeLuxBrightness(lux, webFactor):
  total = luxFactor × webFactor
  base = FLOOR + (maxBrightness - FLOOR) × total
```

`baseBrightness` wordt berekend met de webFactor op moment van lux meting.
Als webFactor daarna verandert → baseBrightness blijft oud.

## Oplossing

Slider change triggert lux herberekening, **maar met 60s cooldown** om te frequente metingen te voorkomen.

## Mechanisme (TimerManager-based, Rule 1.2 compliant)

```
State:
  luxCooldownActive = false   // true tijdens 60s cooldown
  luxMeasurePending = false   // true als slider changed tijdens cooldown

On slider change:
  if (!luxCooldownActive):
    trigger lux measurement
    luxCooldownActive = true
    start 60s timer → cb_luxCooldownExpired
  else:
    luxMeasurePending = true

cb_luxCooldownExpired:
  if (luxMeasurePending):
    trigger lux measurement
    luxMeasurePending = false
    start 60s timer → cb_luxCooldownExpired  // nieuwe cooldown
  else:
    luxCooldownActive = false
```

## Flow Diagram

```
Slider 100%→50%  [t=0]   → lux meting, cooldown start
Slider 50%→30%   [t=10s] → pending=true (in cooldown)
Slider 30%→20%   [t=20s] → pending=true (nog steeds)
Cooldown expires [t=60s] → pending? ja → lux meting, nieuwe cooldown
Slider 20%→80%   [t=90s] → pending=true (in cooldown)
Cooldown expires [t=120s]→ pending? ja → lux meting
```

Resultaat: max 1 meting per 60s, altijd met meest recente webFactor.

## Bestanden te Wijzigen

| # | File | Wijziging |
|---|------|-----------|
| 1 | `lib/ConductManager/Light/LightConduct.h` | Add `luxCooldownActive`, `luxMeasurePending` state |
| 2 | `lib/ConductManager/Light/LightConduct.cpp` | Add `cb_luxCooldownExpired`, `requestLuxWithCooldown()` |
| 3 | `lib/ConductManager/ConductManager.cpp` | `requestLuxMeasurement()` → use cooldown variant |
| 4 | `lib/WebInterfaceManager/WebInterfaceManager.cpp` | Slider change calls `requestLuxMeasurement()` |
| 5 | `lib/Globals/Globals.h` | Version bump |

## Detail per File

### 1. LightConduct.h

```cpp
// Add to class:
static bool luxCooldownActive;
static bool luxMeasurePending;
static void cb_luxCooldownExpired();
static void requestLuxWithCooldown();  // Public API for slider
```

### 2. LightConduct.cpp

```cpp
// Module state
bool LightConduct::luxCooldownActive = false;
bool LightConduct::luxMeasurePending = false;

void LightConduct::requestLuxWithCooldown() {
    if (!luxCooldownActive) {
        cb_luxMeasure();  // Trigger immediate measurement
        luxCooldownActive = true;
        TimerManager::instance().create(60000, 1, cb_luxCooldownExpired);
    } else {
        luxMeasurePending = true;
    }
}

void LightConduct::cb_luxCooldownExpired() {
    if (luxMeasurePending) {
        luxMeasurePending = false;
        cb_luxMeasure();  // Measurement with latest webFactor
        // Restart cooldown
        TimerManager::instance().create(60000, 1, cb_luxCooldownExpired);
    } else {
        luxCooldownActive = false;
    }
}
```

### 3. ConductManager.cpp (regel ~75)

```cpp
void ConductManager::requestLuxMeasurement() {
    LightConduct::requestLuxWithCooldown();  // Use cooldown variant
}
```

### 4. WebInterfaceManager.cpp (handleSetBrightness)

```cpp
void handleSetBrightness(AsyncWebServerRequest *request)
{
    // ... existing code ...
    setWebBrightness(webFactor);
    ConductManager::requestLuxMeasurement();  // Trigger herberekening (met cooldown)
    WebGuiStatus::setBrightness(static_cast<uint8_t>(val));
    // ...
}
```

## Verificatie

1. Slider 100%→20% → lux meting direct
2. Snel slider 20%→50%→80% → max 1 meting na 60s met webFactor=0.8
3. Wacht 2 min, slider 80%→10% → lux meting direct

## Architecture Check

| Rule | Status |
|------|--------|
| 1.1 Conduct/Policy flow | ✅ Via LightConduct |
| 1.2 TimerManager only | ✅ Geen millis() |
| 1.3 cb_ prefix | ✅ cb_luxCooldownExpired |
| 7.3 No new helpers | ✅ Extends existing pattern |
| 19 No polling | ✅ Timer-driven |

## Globals Constant (optioneel)

Indien 60s configureerbaar moet zijn:
```cpp
// Globals.h
static uint32_t luxCooldownMs;  // default 60000

// Globals.cpp  
uint32_t Globals::luxCooldownMs = 60000U;
```

---

**APPROVED?**
