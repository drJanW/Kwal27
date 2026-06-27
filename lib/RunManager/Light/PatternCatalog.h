/**
 * @file PatternCatalog.h
 * @brief LED pattern storage
 * @version 260615E
 * @date 2026-03-05
 */
#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <FastLED.h>
#include <vector>

#include "LightController.h"
// ShiftEnums.h no longer needed - shifts handled in LightRun

class PatternCatalog {
public:
    static PatternCatalog& instance();

    void begin();
    bool isReady() const { return ready_; }
    bool selectRandom();

    String buildJson(const char* source = "context") const;

    bool select(const String& id, String& errorMessage);
    bool selectNext(String& errorMessage);
    bool selectPrev(String& errorMessage);
    bool update(JsonVariantConst body, String& affectedId, String& errorMessage);
    bool remove(JsonVariantConst body, String& affectedId, String& errorMessage);

    const String& activeId() const { return activePatternId_; }
    String firstPatternId() const { return patterns_.empty() ? String() : patterns_.front().id; }

    LightShowParams getActiveParams() const;  // Returns RAW params, shifts applied in LightRun
    bool parseParams(JsonVariantConst src, LightShowParams& out, String& errorMessage) const;
    bool getParamsForId(const String& id, LightShowParams& out) const;
    String getLabelForId(const String& id) const;  // Returns label or empty string if not found

    // PNF for a pattern id. Returns effective value (0 → 1.0f uncalibrated).
    float pnf(const String& id) const;

    // PNF calibration API (used by LightRun)
    uint8_t countUncalibrated() const;
    std::vector<String> getUncalibratedIds() const;
    bool setPnf(const String& id, float value);
    bool saveToSD() const;

private:
    PatternCatalog() = default;

    struct PatternEntry {
        String id;
        String label;
        LightShowParams params;
        float pnf{0.0f};  // 0 = uncalibrated, >0 = calibrated
    };

    bool loadFromSD();

    PatternEntry* findEntry(const String& id);
    const PatternEntry* findEntry(const String& id) const;

    String generateId() const;

    std::vector<PatternEntry> patterns_;
    String activePatternId_;
    bool ready_{false};
};
