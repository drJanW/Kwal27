/**
 * @file WebGuiStatus.cpp
 * @brief Centralized WebGUI state management implementation
 * @version 260101A
 * @date 2026-01-01
 *
 * SSE pushes current state from source modules (LightManager, AudioState).
 * No local caching of brightness/audio - always reads from source.
 */

#include "WebGuiStatus.h"
#include "Globals.h"
#include "Light/PatternCatalog.h"
#include "Light/ColorsCatalog.h"
#include "Light/LightPolicy.h"
#include "LightManager.h"
#include "AudioState.h"
#include "SensorManager.h"
#include <ESPAsyncWebServer.h>

#ifndef WEBIF_LOG_LEVEL
#define WEBIF_LOG_LEVEL 1
#endif

#if WEBIF_LOG_LEVEL
#define WEBIF_LOG(...) PF(__VA_ARGS__)
#else
#define WEBIF_LOG(...) do {} while (0)
#endif

namespace WebGuiStatus {

// ============================================================================
// Atomic state storage
// ============================================================================

namespace {
    // Fragment state - updated by setFragment()
    std::atomic<uint8_t> fragmentDir_{0};
    std::atomic<uint8_t> fragmentFile_{0};
    std::atomic<uint8_t> fragmentScore_{0};
    std::atomic<uint32_t> fragmentDurationMs_{0};
    
    // Pointer to SSE event source (set during begin())
    AsyncEventSource* eventsPtr_ = nullptr;
}

// ============================================================================
// Setters - trigger SSE push (actual values come from getters in pushState)
// ============================================================================

void setBrightness(uint8_t /* value */) {
    // Value ignored - pushState reads from getSliderPct()
    pushState();
}

void setAudioLevel(float /* value */) {
    // Value ignored - pushState reads from getAudioSliderPct()
    pushState();
}

void setFragment(uint8_t dir, uint8_t file, uint8_t score, uint32_t durationMs) {
    fragmentDir_.store(dir, std::memory_order_relaxed);
    fragmentFile_.store(file, std::memory_order_relaxed);
    fragmentScore_.store(score, std::memory_order_relaxed);
    fragmentDurationMs_.store(durationMs, std::memory_order_relaxed);
    pushState();
}

void setFragmentScore(uint8_t score) {
    fragmentScore_.store(score, std::memory_order_relaxed);
    pushState();
}

// ============================================================================
// Getters (for fragment state only - brightness/audio come from LightManager/AudioState)
// ============================================================================

uint8_t getFragmentDir() {
    return fragmentDir_.load(std::memory_order_relaxed);
}

uint8_t getFragmentFile() {
    return fragmentFile_.load(std::memory_order_relaxed);
}

uint8_t getFragmentScore() {
    return fragmentScore_.load(std::memory_order_relaxed);
}

// ============================================================================
// SSE Push functions
// ============================================================================

void pushState() {
    if (!eventsPtr_) return;
    
    // Build state JSON
    // {
    //   "brightness": 128,
    //   "audioLevel": 0.75,
    //   "patternId": "rainbow",
    //   "patternLabel": "Rainbow Fade",
    //   "colorId": "sunset",
    //   "colorLabel": "Warm Sunset",
    //   "fragment": { "dir": 3, "file": 7, "score": 2 }
    // }
    
    // Slider position (0-100) derived from shiftedHi
    int sliderPct = getSliderPct();
    int audioSliderPct = getAudioSliderPct();
    uint8_t dir = fragmentDir_.load(std::memory_order_relaxed);
    uint8_t file = fragmentFile_.load(std::memory_order_relaxed);
    uint8_t score = fragmentScore_.load(std::memory_order_relaxed);
    
    // Get pattern/color from catalogs (use effective ID with fallback to first)
    String patternId = PatternCatalog::instance().activeId();
    if (patternId.isEmpty()) {
        patternId = PatternCatalog::instance().firstPatternId();
    }
    String colorId = ColorsCatalog::instance().getActiveColorId();
    if (colorId.isEmpty()) {
        colorId = ColorsCatalog::instance().firstColorId();
    }
    String patternLabel = PatternCatalog::instance().getLabelForId(patternId);
    String colorLabel = ColorsCatalog::instance().getLabelForId(colorId);
    
    // Build JSON manually for efficiency (avoid ArduinoJson allocation)
    String json;
    json.reserve(450);
    json = F("{\"sliderPct\":");
    json += sliderPct;
    json += F(",\"brightnessLo\":");
    json += Globals::brightnessLo;
    json += F(",\"brightnessHi\":");
    json += Globals::brightnessHi;
    json += F(",\"brightnessMax\":");
    json += MAX_BRIGHTNESS;
    json += F(",\"audioSliderPct\":");
    json += audioSliderPct;
    json += F(",\"volumeLo\":");
    json += String(Globals::volumeLo, 2);
    json += F(",\"volumeHi\":");
    json += String(getVolumeShiftedHi(), 2);
    json += F(",\"volumeMax\":");
    json += String(MAX_VOLUME, 2);
    json += F(",\"patternId\":\"");
    json += patternId;
    json += F("\",\"patternLabel\":\"");
    json += patternLabel;
    json += F("\",\"colorId\":\"");
    json += colorId;
    json += F("\",\"colorLabel\":\"");
    json += colorLabel;
    json += F("\",\"fragment\":{\"dir\":");
    json += dir;
    json += F(",\"file\":");
    json += file;
    json += F(",\"score\":");
    json += score;
    json += F(",\"durationMs\":");
    json += fragmentDurationMs_.load(std::memory_order_relaxed);
    json += F("}}");
    
    eventsPtr_->send(json.c_str(), "state", millis());
    WEBIF_LOG("[SSE] state sliderPct=%d audio=%d pat=%s col=%s\n", sliderPct, audioSliderPct, patternId.c_str(), colorId.c_str());
}

void pushPatterns() {
    if (!eventsPtr_) return;
    
    String json = PatternCatalog::instance().buildJson("manual");
    eventsPtr_->send(json.c_str(), "patterns", millis());
    WEBIF_LOG("[SSE] patterns pushed (%u bytes)\n", json.length());
}

void pushColors() {
    if (!eventsPtr_) return;
    
    String json = ColorsCatalog::instance().buildColorsJson("manual");
    eventsPtr_->send(json.c_str(), "colors", millis());
    WEBIF_LOG("[SSE] colors pushed (%u bytes)\n", json.length());
}

void pushAll() {
    pushPatterns();
    pushColors();
    pushState();
}

// ============================================================================
// Initialization
// ============================================================================

void begin() {
    WEBIF_LOG("[WebGuiStatus] initialized\n");
}

/**
 * @brief Set SSE event source pointer (called from SseManager::setup)
 * @param events Pointer to AsyncEventSource
 */
void setEventSource(AsyncEventSource* events) {
    eventsPtr_ = events;
}

} // namespace WebGuiStatus
