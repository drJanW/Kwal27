# Plan F9: Brightness Slider

## Concept

Slider = directe brightness control (0-100% van Lo..Hi range).

```
slider 0%   → brightness = Globals::brightnessLo
slider 100% → brightness = Globals::brightnessHi
slider 50%  → brightness = map(50, loPct, hiPct, brightnessLo, brightnessHi)
```

## Globals.h Constants

```cpp
inline static constexpr int loPct = 0;
inline static constexpr int hiPct = 100;
inline static constexpr uint8_t brightnessLo = 20;
inline static constexpr uint8_t brightnessHi = 255;
```

## handleSetBrightness Flow (WebInterfaceManager.cpp)

User sleept slider naar `sliderPct`:

```cpp
void handleSetBrightness(AsyncWebServerRequest *request) {
    int sliderPct = request->getParam("value")->value().toInt();
    sliderPct = constrain(sliderPct, Globals::loPct, Globals::hiPct);
    
    // 1. Get current lux (cached, no new measurement)
    float lux = SensorManager::ambientLux();
    
    // 2. Get calendar shift
    uint64_t statusBits = ContextFlags::getFullContextBits();
    float colorMults[COLOR_PARAM_COUNT];
    shiftStore.computeColorMultipliers(statusBits, colorMults);
    int8_t calendarShift = static_cast<int8_t>((colorMults[GLOBAL_BRIGHTNESS] - 1.0f) * 100.0f);
    
    // 3. Calculate webShift to achieve sliderPct
    //    First: what brightness does user want?
    float targetBrightness = MathUtils::mapRange(
        static_cast<float>(sliderPct),
        static_cast<float>(Globals::loPct), static_cast<float>(Globals::hiPct),
        static_cast<float>(Globals::brightnessLo), static_cast<float>(Globals::brightnessHi));
    
    //    Second: what would shiftedHi be with webShift=1.0?
    uint8_t baseShiftedHi = LightPolicy::calcShiftedHi(lux, calendarShift, 1.0f);
    
    //    Third: webShift = targetBrightness / baseShiftedHi
    float webShift = (baseShiftedHi > 0) ? (targetBrightness / baseShiftedHi) : 1.0f;
    setWebShift(webShift);
    
    // 4. Recalculate shiftedHi with new webShift
    uint8_t shiftedHi = LightPolicy::calcShiftedHi(lux, calendarShift, webShift);
    setBrightnessShiftedHi(shiftedHi);
    
    // 5. Apply to LEDs (next updateLightManager will use new shiftedHi)
    WEBIF_LOG("[Web] sliderPct=%d → webShift=%.2f shiftedHi=%u\n", sliderPct, webShift, shiftedHi);
    
    request->send(200, "text/plain", "OK");
}
```

## applyBrightness()

```cpp
void applyBrightness() {
  int sliderPct = getSliderPct();  // 0..100 derived from shiftedHi
  
  uint8_t brightness = map(sliderPct, 
                           Globals::loPct, Globals::hiPct,
                           Globals::brightnessLo, Globals::brightnessHi);
  
  // Audio modulatie (alleen omlaag)
  if (isAudioBusy()) {
    uint16_t audioLevel = getAudioLevelRaw();
    if (audioLevel) {
      float audioModifier = sqrtf(audioLevel / 32768.0f) * 1.2f;
      brightness = static_cast<uint8_t>(brightness * MathUtils::clamp01(audioModifier));
    }
  }
  
  FastLED.setBrightness(brightness);
}
```

## getSliderPct()

Inverse mapping: shiftedHi → sliderPct

```cpp
int getSliderPct() {
  return static_cast<int>(MathUtils::mapRange(
    static_cast<float>(brightnessShiftedHi),
    static_cast<float>(Globals::brightnessLo), static_cast<float>(Globals::brightnessHi),
    static_cast<float>(Globals::loPct), static_cast<float>(Globals::hiPct)));
}
```

## WebShift Voorbeeld

```
Situatie: lux=100, calendarShift=-10% → baseShiftedHi = 180 (met webShift=1.0)

User wil slider op 100%:
  targetBrightness = map(100, 0, 100, 20, 255) = 255
  webShift = 255 / 180 = 1.42
  shiftedHi = calcShiftedHi(lux, calShift, 1.42) = 255

User wil slider op 50%:
  targetBrightness = map(50, 0, 100, 20, 255) = 137
  webShift = 137 / 180 = 0.76
  shiftedHi = calcShiftedHi(lux, calShift, 0.76) = 137
```

**webShift kan > 1.0 zijn** om andere shifts te overrulen.

## SSE Payload

```json
{
  "sliderPct": 70,
  "brightnessLo": 20,
  "brightnessHi": 255
}
```

`sliderPct` = huidige actuele brightness als percentage van Lo..Hi range.

## Files to Modify

1. `LightManager.cpp` - webShift variable, getWebShift(), setWebShift(), getSliderPct(), applyBrightness()
2. `LightManager.h` - function declarations
3. `LightPolicy.cpp` - calcShiftedHi(lux, calShift, webShift)
4. `LightRun.cpp` - pass getWebShift() to calcShiftedHi
5. `WebInterfaceManager.cpp` - handleSetBrightness met volledige flow
6. `WebGuiStatus.cpp` - sliderPct naar SSE
7. `brightness.js` - slider UI

## Status

- [x] Concept defined
- [x] WebShift als multiplier (kan > 1.0)
- [x] Add loPct/hiPct to Globals.h
- [x] Rename applyFinalBrightness → applyBrightness
- [x] LightManager.cpp/h - webShift, getSliderPct()
- [x] LightPolicy.cpp - calcShiftedHi met webShift parameter
- [x] LightRun.cpp - pass webShift
- [ ] WebInterfaceManager.cpp - CORRECTE handleSetBrightness
- [x] WebGuiStatus.cpp - sliderPct
- [x] brightness.js
- [ ] Testing
