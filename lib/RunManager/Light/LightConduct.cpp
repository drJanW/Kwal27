/**
 * @file LightConduct.cpp
 * @brief LED show state management implementation
 * @version 260102F
 * @date 2026-01-02
 *
 * Implements LED show orchestration: manages pattern/color state, applies
 * context-based shifts, handles lux measurement cycles with LED blackout,
 * and coordinates with ColorsCatalog, PatternCatalog, and ShiftTable.
 */

#if 0
#include "LightConduct.h"

#include "Globals.h"
#include "LightPolicy.h"
#include "LightManager.h"
#include "SensorManager.h"
#include "TimerManager.h"
#include "ColorsCatalog.h"
#include "PatternCatalog.h"
#include "ShiftTable.h"
#include "ContextFlags.h"
#include "ContextModels.h"
#include "Alert/AlertRGB.h"
#include "Alert/AlertState.h"
#include "WebGuiStatus.h"
#include <FastLED.h>

namespace {

// Lux measurement state
bool luxMeasureActive = false;
bool luxRequestPending = false;      // Slider requested measurement (B6)
bool luxInCooldown = false;          // 100ms cooldown active (B6)

uint32_t currentIntervalMs = 0;
float currentIntensity = 0.0f;
uint8_t currentPaletteId = 0;
bool timerActive = false;
bool shiftTimerActive = false;

// Source tracking for pattern and color selection
LightSource patternSource = LightSource::CONTEXT;
LightSource colorSource = LightSource::CONTEXT;

// Shift change tracking - only reapply when context changes
uint64_t lastStatusBits = 0;

ShiftTable& shiftTable = ShiftTable::instance();

const char* sourceToString(LightSource src) {
    switch (src) {
        case LightSource::MANUAL:   return "manual";
        case LightSource::CALENDAR: return "calendar";
        default:                    return "context";
    }
}

ColorsCatalog &getColorsCatalog() {
    ColorsCatalog &catalog = ColorsCatalog::instance();
    if (!catalog.isReady()) {
        catalog.begin();
    }
    return catalog;
}

PatternCatalog &getPatternCatalog() {
    PatternCatalog &catalog = PatternCatalog::instance();
    if (!catalog.isReady()) {
        catalog.begin();
    }
    return catalog;
}

void applyLightshowUpdate() {
    (void)currentIntensity;
    (void)currentPaletteId;
    // TODO: Implement distance-driven RGB lightshow update logic.
}

bool scheduleAnimation(uint32_t intervalMs) {
    TimerCallback cb = LightConduct::cb_animation;
    // Use restart() - called on every pattern change, timer may already exist
    if (!timers.restart(intervalMs, 1, cb)) {
        PF("[LightConduct] Failed to create animation timer (%lu ms)\n",
           static_cast<unsigned long>(intervalMs));
        timerActive = false;
        return false;
    }
    timerActive = true;
    currentIntervalMs = intervalMs;
    return true;
}

void stopAnimation() {
    if (timerActive) {
        timers.cancel(LightConduct::cb_animation);
        timerActive = false;
    }
    currentIntervalMs = 0;
    currentIntensity = 0.0f;
    currentPaletteId = 0;
}

bool scheduleShiftTimer() {
    TimerCallback cb = LightConduct::cb_shiftTimer;
    // Use restart() - called repeatedly to reschedule shift checks
    if (!timers.restart(Globals::shiftCheckIntervalMs, 1, cb)) {
        PF("[LightConduct] Failed to create shift timer (%lu ms)\n",
           static_cast<unsigned long>(Globals::shiftCheckIntervalMs));
        shiftTimerActive = false;
        return false;
    }
    shiftTimerActive = true;
    return true;
}

} // namespace

void LightConduct::plan() {
    stopAnimation();
    
    // Load shifts from SD
    shiftTable.begin();

    // Preload pattern/color catalogs while SD is still uncontended.
    // This avoids lazy SD reads inside web request handlers during audio playback.
    getPatternCatalog();
    getColorsCatalog();
    
    // Apply immediately and start periodic check timer
    lastStatusBits = ContextFlags::getFullContextBits();
    applyToLights();
    scheduleShiftTimer();
    
    // Periodic lux measurement (Light's responsibility)
    timers.create(Globals::luxMeasurementIntervalMs, 0, LightConduct::cb_luxMeasure);
    
    // NOTE: Status flash handled by AlertRGB reminder system (61 min interval)
    
    PL("[Conduct][Plan] Light shift system initialized");
}

void LightConduct::updateDistance(float distanceMm) {
    uint32_t intervalMs = 0;
    float intensity = 0.0f;
    uint8_t paletteId = 0;

    if (!LightPolicy::distanceAnimationFor(distanceMm, intervalMs, intensity, paletteId)) {
        stopAnimation();
        return;
    }

    if (intervalMs == 0) {
        intervalMs = Globals::lightFallbackIntervalMs;
    }

    currentIntervalMs = intervalMs;
    currentIntensity = intensity;
    currentPaletteId = paletteId;

    if (!timerActive) {
        scheduleAnimation(intervalMs);
    }
}

void LightConduct::cb_animation() {
    applyLightshowUpdate();
    timerActive = false;
}

void LightConduct::cb_shiftTimer() {
    shiftTimerActive = false;
    
    uint64_t statusBits = ContextFlags::getFullContextBits();
    if (statusBits != lastStatusBits) {
        lastStatusBits = statusBits;
        applyToLights();
        PF("[LightConduct] Shifts updated (status=0x%llX)\n", statusBits);
    }
    
    scheduleShiftTimer();
}

void LightConduct::cb_luxMeasure() {
    // Skip if no lux sensor present (preserves boot default brightness)
    if (!AlertState::isLuxSensorOk()) return;
    
    // Step 1: Enter measurement enable (LEDs off for accurate sensor read)
    lightManager.setMeasurementEnabled(true);
    luxMeasureActive = true;
    // Step 2: Schedule delayed read after LEDs settle
    timers.create(Globals::luxMeasurementDelayMs, 1, LightConduct::cb_luxMeasureRead);
}

void LightConduct::cb_luxMeasureRead() {
    if (!luxMeasureActive) return;  // Guard: measurement was cancelled
    luxMeasureActive = false;
    
    // Step 3: Read sensor
    SensorManager::performLuxMeasurement();
    float lux = SensorManager::ambientLux();
    
    // Step 4: Get calendar brightness shift and compute shiftedHi in one formula
#ifndef DISABLE_SHIFTS
    uint64_t statusBits = ContextFlags::getFullContextBits();
    float colorMults[COLOR_PARAM_COUNT];
    shiftTable.computeColorMultipliers(statusBits, colorMults);
    int8_t calendarShift = static_cast<int8_t>((colorMults[GLOBAL_BRIGHTNESS] - 1.0f) * 100.0f);
#else
    int8_t calendarShift = 0;
#endif
    
    uint8_t shiftedHi = LightPolicy::calcShiftedHi(lux, calendarShift, getWebShift());
    setBrightnessShiftedHi(shiftedHi);
    
    PF("[LightConduct] Lux=%.1f calShift=%d webShift=%.2f → shiftedHi=%u\n", lux, calendarShift, getWebShift(), shiftedHi);
    
    // Step 5: Apply pattern/color shifts (brightness already done above)
    applyToLights();
    lightManager.setMeasurementEnabled(false);
    WebGuiStatus::pushState();
    
    // B6: Start cooldown, check for pending slider request
    luxInCooldown = true;
    timers.create(100, 1, LightConduct::cb_cooldownExpired);
    if (luxRequestPending) {
        timers.create(100, 1, LightConduct::cb_tryLuxMeasure);
    }
}

void LightConduct::cb_cooldownExpired() {
    luxInCooldown = false;
}

// B6: Slider-triggered lux measurement with debounce + 100ms cooldown
void LightConduct::requestLuxMeasurement() {
    luxRequestPending = true;
    cb_tryLuxMeasure();
}

// F9: Route webShift through Conduct to LightManager
void LightConduct::setWebBrightnessModifier(float multiplier) {
    setWebShift(multiplier);
}

void LightConduct::cb_tryLuxMeasure() {
    if (luxMeasureActive) return;   // Already measuring
    if (!luxRequestPending) return; // No pending request
    
    if (luxInCooldown) {
        // In cooldown - schedule retry after full cooldown period
        timers.create(100, 1, LightConduct::cb_tryLuxMeasure);
        return;
    }
    
    luxRequestPending = false;
    cb_luxMeasure();  // Start measurement
}

LightSource LightConduct::getPatternSource() {
    return patternSource;
}

LightSource LightConduct::getColorSource() {
    return colorSource;
}

bool LightConduct::patternRead(String &payload, String &activePatternId) {
    PatternCatalog &catalog = getPatternCatalog();
    // Pass source from Conduct layer to catalog for JSON building
    payload = catalog.buildJson(sourceToString(patternSource));
    if (payload.isEmpty()) {
        return false;
    }
    activePatternId = catalog.activeId();
    return true;
}

bool LightConduct::colorRead(String &payload, String &activeColorId) {
    ColorsCatalog &catalog = getColorsCatalog();
    // Pass source from Conduct layer to catalog for JSON building
    payload = catalog.buildColorsJson(sourceToString(colorSource));
    if (payload.isEmpty()) {
        return false;
    }
    activeColorId = catalog.getActiveColorId();
    return true;
}

bool LightConduct::selectPattern(const String &id, String &errorMessage) {
    PatternCatalog &catalog = getPatternCatalog();
    if (!catalog.select(id, errorMessage)) {
        return false;
    }
    // Web UI selection = manual source (empty id clears to context)
    patternSource = id.isEmpty() ? LightSource::CONTEXT : LightSource::MANUAL;
    applyToLights();
    return true;
}

bool LightConduct::selectNextPattern(String &errorMessage) {
    PatternCatalog &catalog = getPatternCatalog();
    if (!catalog.selectNext(errorMessage)) {
        return false;
    }
    patternSource = LightSource::MANUAL;
    AlertRGB::stopFlashing();  // User action overrides error flash
    applyToLights();
    return true;
}

bool LightConduct::selectPrevPattern(String &errorMessage) {
    PatternCatalog &catalog = getPatternCatalog();
    if (!catalog.selectPrev(errorMessage)) {
        return false;
    }
    patternSource = LightSource::MANUAL;
    AlertRGB::stopFlashing();  // User action overrides error flash
    applyToLights();
    return true;
}

bool LightConduct::updatePattern(JsonVariantConst body, String &affectedId, String &errorMessage) {
    PatternCatalog &catalog = getPatternCatalog();
    if (!catalog.update(body, affectedId, errorMessage)) {
        return false;
    }
    // Update doesn't change source
    applyToLights();
    return true;
}

bool LightConduct::deletePattern(JsonVariantConst body, String &affectedId, String &errorMessage) {
    PatternCatalog &catalog = getPatternCatalog();
    if (!catalog.remove(body, affectedId, errorMessage)) {
        return false;
    }
    // Delete may clear active, revert to context
    if (catalog.activeId().isEmpty()) {
        patternSource = LightSource::CONTEXT;
    }
    applyToLights();
    return true;
}

bool LightConduct::selectColor(const String &id, String &errorMessage) {
    if (!getColorsCatalog().selectColor(id, errorMessage)) {
        return false;
    }
    // Web UI selection = manual source (empty id clears to context)
    colorSource = id.isEmpty() ? LightSource::CONTEXT : LightSource::MANUAL;
    applyToLights();
    return true;
}

bool LightConduct::selectNextColor(String &errorMessage) {
    if (!getColorsCatalog().selectNextColor(errorMessage)) {
        return false;
    }
    colorSource = LightSource::MANUAL;
    AlertRGB::stopFlashing();  // User action overrides error flash
    applyToLights();
    return true;
}

bool LightConduct::selectPrevColor(String &errorMessage) {
    if (!getColorsCatalog().selectPrevColor(errorMessage)) {
        return false;
    }
    colorSource = LightSource::MANUAL;
    AlertRGB::stopFlashing();  // User action overrides error flash
    applyToLights();
    return true;
}

bool LightConduct::updateColor(JsonVariantConst body, String &affectedId, String &errorMessage) {
    if (!getColorsCatalog().updateColor(body, affectedId, errorMessage)) {
        return false;
    }
    // Update doesn't change source
    applyToLights();
    return true;
}

bool LightConduct::deleteColorSet(JsonVariantConst body, String &affectedId, String &errorMessage) {
    if (!getColorsCatalog().deleteColorSet(body, affectedId, errorMessage)) {
        return false;
    }
    // Delete may clear active, revert to context
    if (getColorsCatalog().getActiveColorId().isEmpty()) {
        colorSource = LightSource::CONTEXT;
    }
    applyToLights();
    return true;
}

bool LightConduct::previewPattern(JsonVariantConst body, String &errorMessage) {
    PatternCatalog& patternCatalog = getPatternCatalog();
    ColorsCatalog& colorCatalog = getColorsCatalog();

    JsonObjectConst obj = body.as<JsonObjectConst>();
    if (obj.isNull()) {
        errorMessage = F("invalid payload");
        PF("[LightConduct] previewPattern reject: body not object\n");
        return false;
    }

    // Parse pattern params from body.pattern.params or body.params
    LightShowParams params;
    JsonVariantConst patternVariant = obj["pattern"];
    JsonVariantConst paramsVariant;
    if (!patternVariant.isNull() && patternVariant.is<JsonObjectConst>()) {
        JsonObjectConst patternObj = patternVariant.as<JsonObjectConst>();
        paramsVariant = patternObj["params"];
        if (paramsVariant.isNull()) {
            paramsVariant = patternObj;
        }
    } else {
        paramsVariant = obj["params"];
    }

    if (paramsVariant.isNull() || !patternCatalog.parseParams(paramsVariant, params, errorMessage)) {
        if (errorMessage.isEmpty()) {
            errorMessage = F("pattern params missing or invalid");
        }
        PF("[LightConduct] previewPattern reject: %s\n", errorMessage.c_str());
        return false;
    }

    // Parse colors from body.color
    JsonVariantConst colorVariant = obj["color"];
    if (!colorVariant.isNull()) {
        CRGB colorA, colorB;
        String colorError;
        if (ColorsCatalog::parseColorPayload(colorVariant, colorA, colorB, colorError)) {
            params.RGB1 = colorA;
            params.RGB2 = colorB;
        } else {
            // Color parsing failed, use current active colors
            LightShowParams activeParams = patternCatalog.getActiveParams();
            params.RGB1 = activeParams.RGB1;
            params.RGB2 = activeParams.RGB2;
        }
    } else {
        // No color in body, use current active colors
        LightShowParams activeParams = patternCatalog.getActiveParams();
        params.RGB1 = activeParams.RGB1;
        params.RGB2 = activeParams.RGB2;
    }

    PF("[LightConduct] previewPattern applied\n");
    PlayLightShow(params);
    return true;
}

bool LightConduct::previewColor(JsonVariantConst body, String &errorMessage) {
    return getColorsCatalog().previewColors(body, errorMessage);
}

// --- Calendar-driven selection requests ---

void LightConduct::applyPattern(uint8_t patternId) {
    PatternCatalog& catalog = getPatternCatalog();
    String errorMessage;
    if (patternId == 0) {
        // Calendar has no pattern - keep current pattern, don't clear
        // This preserves boot random or user-selected pattern
        PF("[LightConduct] Calendar: no pattern, keeping current\n");
    } else {
        // Calendar specifies pattern, select it
        if (catalog.select(String(patternId), errorMessage)) {
            patternSource = LightSource::CALENDAR;
            PF("[LightConduct] Calendar: pattern %u selected\n", static_cast<unsigned>(patternId));
        } else {
            PF("[LightConduct] Calendar: pattern %u failed: %s\n", 
               static_cast<unsigned>(patternId), errorMessage.c_str());
        }
    }
    applyToLights();
}

void LightConduct::applyColor(uint8_t colorId) {
    ColorsCatalog& catalog = getColorsCatalog();
    String errorMessage;
    if (colorId == 0) {
        // Calendar has no color - keep current (boot random or previous selection)
        colorSource = LightSource::CONTEXT;
        PF("[LightConduct] Calendar: no color, keeping current\n");
    } else {
        // Calendar specifies color, select it
        if (catalog.selectColor(String(colorId), errorMessage)) {
            colorSource = LightSource::CALENDAR;
            PF("[LightConduct] Calendar: color %u selected\n", static_cast<unsigned>(colorId));
        } else {
            PF("[LightConduct] Calendar: color %u failed: %s\n",
               static_cast<unsigned>(colorId), errorMessage.c_str());
        }
    }
    applyToLights();
}

// --- TodayContext request methods ---

bool LightConduct::describePatternById(uint8_t id, LightPattern& out) {
    if (id == 0) {
        return false;
    }
    PatternCatalog& catalog = getPatternCatalog();
    String idStr = String(id);
    LightShowParams params;
    if (!catalog.getParamsForId(idStr, params)) {
        return false;
    }
    out.valid = true;
    out.id = id;
    String label = catalog.getLabelForId(idStr);
    out.label = label.isEmpty() ? idStr : label;  // Use actual label, fallback to ID if empty
    out.color_cycle_sec = static_cast<float>(params.colorCycleSec);
    out.bright_cycle_sec = static_cast<float>(params.brightCycleSec);
    out.fade_width = params.fadeWidth;
    out.min_brightness = static_cast<float>(params.minBrightness);
    out.gradient_speed = params.gradientSpeed;
    out.center_x = params.centerX;
    out.center_y = params.centerY;
    out.radius = params.radius;
    out.window_width = static_cast<float>(params.windowWidth);
    out.radius_osc = params.radiusOsc;
    out.x_amp = params.xAmp;
    out.y_amp = params.yAmp;
    out.x_cycle_sec = static_cast<float>(params.xCycleSec);
    out.y_cycle_sec = static_cast<float>(params.yCycleSec);
    return true;
}

bool LightConduct::describeActivePattern(LightPattern& out) {
    PatternCatalog& catalog = getPatternCatalog();
    String activeId = catalog.activeId();
    // If no explicit selection, use fallback to first pattern (matches getActiveParams behavior)
    if (activeId.isEmpty()) {
        activeId = catalog.firstPatternId();
    }
    if (activeId.isEmpty()) {
        return false;
    }
    // Parse numeric ID
    uint8_t numericId = static_cast<uint8_t>(activeId.toInt());
    if (numericId == 0) {
        return false;
    }
    return describePatternById(numericId, out);
}

bool LightConduct::describeColorById(uint8_t id, LightColor& out) {
    if (id == 0) {
        return false;
    }
    ColorsCatalog& catalog = getColorsCatalog();
    String label;
    CRGB colorA, colorB;
    if (!catalog.getColorById(String(id), label, colorA, colorB)) {
        return false;
    }
    out.valid = true;
    out.id = id;
    out.label = label;
    out.colorA.r = colorA.r;
    out.colorA.g = colorA.g;
    out.colorA.b = colorA.b;
    out.colorB.r = colorB.r;
    out.colorB.g = colorB.g;
    out.colorB.b = colorB.b;
    return true;
}

bool LightConduct::describeActiveColor(LightColor& out) {
    ColorsCatalog& catalog = getColorsCatalog();
    String activeId = catalog.getActiveColorId();
    // If no explicit selection, use fallback to first color (matches getActiveColors behavior)
    if (activeId.isEmpty()) {
        activeId = catalog.firstColorId();
    }
    if (activeId.isEmpty()) {
        return false;
    }
    uint8_t numericId = static_cast<uint8_t>(activeId.toInt());
    if (numericId == 0) {
        return false;
    }
    return describeColorById(numericId, out);
}

// --- Apply current pattern+color to lights ---

namespace {
// HSV color shift helper (copied from ColorsCatalog for independence)
CRGB shiftColorHSV(const CRGB& rgb, int hueShift, int satShift, int valShift) {
    CHSV hsv = rgb2hsv_approximate(rgb);
    hsv.h += hueShift;
    if (satShift >= 0)
        hsv.s = qadd8(hsv.s, static_cast<uint8_t>(satShift));
    else
        hsv.s = qsub8(hsv.s, static_cast<uint8_t>(-satShift));
    if (valShift >= 0)
        hsv.v = qadd8(hsv.v, static_cast<uint8_t>(valShift));
    else
        hsv.v = qsub8(hsv.v, static_cast<uint8_t>(-valShift));
    return CRGB(hsv);
}
} // namespace

void LightConduct::applyToLights() {
    // Get RAW pattern params from PatternCatalog (no shifts applied)
    PatternCatalog& patternCatalog = getPatternCatalog();
    LightShowParams params = patternCatalog.getActiveParams();

    // Apply pattern shifts here in Conduct layer (not in Catalog)
#ifndef DISABLE_SHIFTS
    {
        uint64_t statusBits = ContextFlags::getFullContextBits();
        float patMults[PAT_PARAM_COUNT];
        shiftTable.computePatternMultipliers(statusBits, patMults);
        
        // Apply multipliers to each parameter (clamp to reasonable ranges)
        params.colorCycleSec  = static_cast<uint8_t>(constrain(params.colorCycleSec * patMults[PAT_COLOR_CYCLE], 1, 255));
        params.brightCycleSec = static_cast<uint8_t>(constrain(params.brightCycleSec * patMults[PAT_BRIGHT_CYCLE], 1, 255));
        params.fadeWidth      = params.fadeWidth * patMults[PAT_FADE_WIDTH];
        params.minBrightness  = static_cast<uint8_t>(constrain(params.minBrightness * patMults[PAT_MIN_BRIGHT], 0, 255));
        params.gradientSpeed  = params.gradientSpeed * patMults[PAT_GRADIENT_SPEED];
        params.centerX        = params.centerX * patMults[PAT_CENTER_X];
        params.centerY        = params.centerY * patMults[PAT_CENTER_Y];
        params.radius         = params.radius * patMults[PAT_RADIUS];
        params.windowWidth    = static_cast<int>(params.windowWidth * patMults[PAT_WINDOW_WIDTH]);
        params.radiusOsc      = params.radiusOsc * patMults[PAT_RADIUS_OSC];
        params.xAmp           = params.xAmp * patMults[PAT_X_AMP];
        params.yAmp           = params.yAmp * patMults[PAT_Y_AMP];
        params.xCycleSec      = static_cast<uint8_t>(constrain(params.xCycleSec * patMults[PAT_X_CYCLE], 1, 255));
        params.yCycleSec      = static_cast<uint8_t>(constrain(params.yCycleSec * patMults[PAT_Y_CYCLE], 1, 255));
    }
#endif

    // Get colors from ColorsCatalog (RAW, no shifts)
    ColorsCatalog& colorsCatalog = getColorsCatalog();
    CRGB colorA, colorB;
    colorsCatalog.getActiveColors(colorA, colorB);

    // Apply color shifts here in Conduct layer (not in Catalog) - identical to pattern shifts
#ifndef DISABLE_SHIFTS
    {
        uint64_t statusBits = ContextFlags::getFullContextBits();
        float colorMults[COLOR_PARAM_COUNT];
        shiftTable.computeColorMultipliers(statusBits, colorMults);
        
        // Convert multipliers to HSV shift values and apply (H/S only, no V)
        auto multToShift = [](float mult, float scale) -> int {
            return static_cast<int>((mult - 1.0f) * scale);
        };
        
        int aHueShift = multToShift(colorMults[COLOR_A_HUE], 256.0f);
        int aSatShift = multToShift(colorMults[COLOR_A_SAT], 255.0f);
        if (aHueShift != 0 || aSatShift != 0) {
            colorA = shiftColorHSV(colorA, aHueShift, aSatShift, 0);
        }
        
        int bHueShift = multToShift(colorMults[COLOR_B_HUE], 256.0f);
        int bSatShift = multToShift(colorMults[COLOR_B_SAT], 255.0f);
        if (bHueShift != 0 || bSatShift != 0) {
            colorB = shiftColorHSV(colorB, bHueShift, bSatShift, 0);
        }
        
        // Brightness shift: only apply if not already computed by calcShiftedHi
        // When called from cb_luxMeasureRead, brightness is already set
        // When called from other places (pattern change etc), apply it here
        if (getBrightnessShiftedHi() == getBrightnessUnshiftedHi()) {
            // Not yet shifted, apply now
            setBrightnessShiftedHi(getBrightnessUnshiftedHi() * colorMults[GLOBAL_BRIGHTNESS]);
        }
    }
#endif
    
    params.RGB1 = colorA;
    params.RGB2 = colorB;

    // Log and apply
    String patternId = patternCatalog.activeId();
    String colorId = colorsCatalog.getActiveColorId();
    PF("[LightConduct] Apply pattern=%s color=%s rgb1=%02X%02X%02X rgb2=%02X%02X%02X\n",
       patternId.isEmpty() ? "<default>" : patternId.c_str(),
       colorId.isEmpty() ? "<default>" : colorId.c_str(),
       params.RGB1.r, params.RGB1.g, params.RGB1.b,
       params.RGB2.r, params.RGB2.g, params.RGB2.b);

    PlayLightShow(params);
}

void LightConduct::reapplyCurrentShow() {
    applyToLights();
}

#endif
