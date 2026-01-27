# Brightness Slider Postmortem

**Probleem**: 4 dagen, $150+ tokens, slider werkte niet  
**Datum**: 2025-01-05  
**Versie**: 260105I

---

## Het Kernprobleem

### Mapping Sets Regel

Mappings moeten **consistente sets** gebruiken:

| Set | Lo | Hi | Beschrijving |
|-----|----|----|--------------|
| Genormaliseerd | 0.0f | 1.0f | Percentages, modifier |
| Hardware | Min (6) | Max (242) | Fysieke grenzen |
| Operationeel | Lo (70) | Hi (242) | Werkbereik |
| Lux | luxMin (0) | luxMax (800) | Sensor range |
| Shift | shiftLo (-10) | shiftHi (+10) | Percentage verschuiving |

**REGEL**: Je mag NIET mixen tussen sets.

```cpp
// ❌ FOUT - mixt Lo met Max
map(x, 0, 1, Lo, Max)

// ❌ FOUT - mixt Min met Hi  
map(x, 0, 1, Min, shiftedHi)

// ✓ CORRECT - consistente set
map(x, 0, 1, Lo, Hi)
map(lux, luxMin, luxMax, shiftLo, shiftHi)
```

---

## Terminologie (Glossary)

| Term | Type | Betekenis | Voorbeeld |
|------|------|-----------|-----------|
| **shift** | int | Percentage verschuiving | -5, +3, 0 |
| **multiplier** | float | 1 + (shift / 100) | 0.95, 1.03, 1.00 |
| **modifier** | float | User attenuation 0..1 | slider positie |
| **Lo, Hi** | const | Operationele grenzen | 70, 242 |
| **Min, Max** | const | Hardware grenzen | 6, 242 |
| **luxMin, luxMax** | const | Sensor grenzen | 0, 800 |
| **shiftLo, shiftHi** | const | Shift grenzen | -10, +10 |

### Conversie: shift → multiplier

```
multiplier = 1 + (shift / 100)

shift = +5  → multiplier = 1.05
shift = -3  → multiplier = 0.97
shift = 0   → multiplier = 1.00
```

---

## De Wiskunde

### Stap 1: Lux → luxShift

```
luxShift = map(lux, luxMin, luxMax, shiftLo, shiftHi)
         = map(lux, 0, 800, -10, +10)
```

| lux | luxShift |
|-----|----------|
| 0 | -10 |
| 400 | 0 |
| 800 | +10 |

### Stap 2: luxShift → luxMultiplier

```
luxMultiplier = 1 + (luxShift / 100)
```

| luxShift | luxMultiplier |
|----------|---------------|
| -10 | 0.90 |
| 0 | 1.00 |
| +10 | 1.10 |

### Stap 3: Calendar → calendarMultiplier

```
calendarShift = uit CSV (bijv. +5)
calendarMultiplier = 1 + (calendarShift / 100) = 1.05
```

### Stap 4: Combineer multipliers

```
combinedMultiplier = luxMultiplier × calendarMultiplier
```

| lux | luxMultiplier | calendarMultiplier | luxMultiplier × calendarMultiplier |
|-----|---------------|-------------------|-----------------------------------|
| 0 | 0.90 | 1.05 | 0.945 |
| 400 | 1.00 | 1.05 | 1.050 |
| 800 | 1.10 | 1.05 | 1.155 |

### Stap 5: Map naar Lo..Hi

```
shiftedLo = Lo × (luxMultiplier × calendarMultiplier)
shiftedHi = Hi × (luxMultiplier × calendarMultiplier)
```

| luxMultiplier × calendarMultiplier | shiftedLo | shiftedHi |
|-----------------------------------|-----------|-----------|
| 0.945 | 66 | 229 |
| 1.050 | 74 | 254 → 242 (clamp) |
| 1.155 | 81 | 280 → 242 (clamp) |

**Opmerking**: shiftedHi wordt geclampt naar Max (242) om hardware overflow te voorkomen.

### Stap 6: Slider → brightness

```
brightness = map(modifier, 0, 1, shiftedLo, shiftedHi)
```

| modifier | brightness (bij totalMult=0.945) |
|----------|----------------------------------|
| 0.0 | 66 (shiftedLo) |
| 0.5 | 148 |
| 1.0 | 229 (shiftedHi) |

---

## Flow Diagram

```
[Lux Sensor]                    [Calendar CSV]
     │                               │
     ▼                               ▼
    lux                         calendarShift
     │                               │
     ▼                               ▼
luxShift = map(lux,            calendarMultiplier =
  luxMin, luxMax,                1 + (calendarShift/100)
  shiftLo, shiftHi)                  │
     │                               │
     ▼                               │
luxMultiplier =                      │
  1 + (luxShift/100)                 │
     │                               │
     └───────────┬───────────────────┘
                 ▼
     luxMultiplier × calendarMultiplier
                 │
     ┌───────────┴───────────┐
     ▼                       ▼
shiftedLo =                        shiftedHi =
  Lo × luxMultiplier ×              min(Hi × luxMultiplier ×
       calendarMultiplier                calendarMultiplier, Max)
     │                       │
     └───────────┬───────────┘
                 ▼
          [Slider: modifier 0..1]
                 │
                 ▼
    brightness = map(modifier, 0, 1, shiftedLo, shiftedHi)
                 │
                 ▼
         FastLED.setBrightness(brightness)
```

---

## Rekenvoorbeeld: Donkere Kamer, Feestdag

**Gegeven**:
- lux = 50 (donker)
- calendarShift = +8 (feestdag)
- modifier = 0.7 (slider op 70%)

**Berekening**:

```
1. luxShift = map(50, 0, 800, -10, +10)
            = -10 + (50/800) × 20
            = -10 + 1.25
            = -8.75

2. luxMultiplier = 1 + (-8.75/100) = 0.9125

3. calendarMultiplier = 1 + (8/100) = 1.08

4. luxMultiplier × calendarMultiplier = 0.9125 × 1.08 = 0.9855

5. shiftedLo = 70 × 0.9855 = 69
   shiftedHi = 242 × 0.9855 = 238

6. brightness = map(0.7, 0, 1, 69, 238)
              = 69 + 0.7 × (238 - 69)
              = 69 + 118
              = 187
```

**Resultaat**: brightness = 187

---

## Wat er mis was (originele code)

```cpp
// LightPolicy.cpp - FOUT
mapRange(luxFactor, 0.0f, 1.0f, brightnessLo, maxBrightness)
//                              ^^^^^^^^^^^   ^^^^^^^^^^^^^
//                              Lo (70)       Max (242)
//                              MIXT SETS!
```

De code mixte Lo (operationeel) met Max (hardware) in één mapping.

---

## Lessen Geleerd

1. **Sets zijn heilig** - Mapping tussen incompatibele sets is altijd fout
2. **shift vs multiplier** - shift is integer percentage, multiplier = 1 + (shift/100)
3. **Multipliers composeren** - vermenigvuldigen, niet optellen
4. **Lo ≠ Min, Hi ≠ Max** - Operationeel bereik ≠ hardware bereik
5. **Clamp alleen aan het eind** - shiftedHi naar Max, niet eerder
6. **Terminologie is architectuur** - Slordig taalgebruik = slordig code

---

## TODO

- [ ] Voeg `brightnessHi = 242U` toe aan Globals.h
- [ ] Voeg `luxMin = 0.0f` toe aan Globals.h
- [ ] Voeg `luxShiftLo`, `luxShiftHi` toe aan Globals.h
- [ ] Refactor `calcUnshiftedHi` naar shift-gebaseerd model
- [ ] LightManager: bereken shiftedLo én shiftedHi
- [ ] LightManager: map(modifier, 0, 1, shiftedLo, shiftedHi)
- [ ] Verwijder luxBeta (vervangen door shift model)
- [ ] Update glossary_slider_semantics.md ✓
