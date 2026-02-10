/**
 * @file PatternCatalog.h
 * @brief LED pattern storage
 * @version 260201A
 * @date 2026-02-01
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

private:
    PatternCatalog() = default;

    struct PatternEntry {
        String id;
        String label;
        LightShowParams params;
    };

    bool loadFromSD();
    bool saveToSD() const;

    PatternEntry* findEntry(const String& id);
    const PatternEntry* findEntry(const String& id) const;

    String generateId() const;

    std::vector<PatternEntry> patterns_;
    String activePatternId_;
    bool ready_{false};
};
