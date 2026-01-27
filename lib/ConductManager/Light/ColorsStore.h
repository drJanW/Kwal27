/**
 * @file ColorsStore.h
 * @brief LED color palette storage
 * @version 251231E
 * @date 2025-12-31
 *
 * Stores and manages LED color palettes loaded from CSV configuration.
 * Provides color selection, CRUD operations for color sets, and JSON
 * serialization for web interface. Single source of truth for color data.
 */

#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <FastLED.h>
#include <vector>

#include "LightManager.h"

class ColorsStore {
public:
    static ColorsStore& instance();

    void begin();
    bool isReady() const;

    String buildColorsJson(const char* source = "context") const;

    // Color selection (data only, no side effects)
    bool selectColor(const String& id, String& errorMessage);
    bool selectNextColor(String& errorMessage);
    bool selectPrevColor(String& errorMessage);

    bool updateColor(JsonVariantConst body, String& affectedId, String& errorMessage);
    bool deleteColorSet(JsonVariantConst body, String& affectedId, String& errorMessage);

    bool previewColors(JsonVariantConst body, String& errorMessage);

    const String& getActiveColorId() const { return activeColorId_; }
    String firstColorId() const { return colors_.empty() ? String() : colors_.front().id; }

    // Direct color lookup by ID (returns false if not found)
    bool getColorById(const String& id, String& label, CRGB& colorA, CRGB& colorB) const;

    // Get label for ID (returns empty string if not found)
    String getLabelForId(const String& id) const;

    // Get active colors (or defaults if none selected)
    void getActiveColors(CRGB& colorA, CRGB& colorB) const;

    // Parse color payload from JSON (public for LightConduct::previewPattern)
    static bool parseColorPayload(JsonVariantConst src, CRGB& a, CRGB& b, String& errorMessage);

private:
    ColorsStore() = default;

    struct ColorEntry {
        String id;
        String label;
        CRGB colorA;
        CRGB colorB;
    };

    bool loadColorsFromSD();
    bool saveColorsToSD() const;

    const ColorEntry* findColor(const String& id) const;
    ColorEntry* findColor(const String& id);

    static bool parseHexColor(const String& hex, CRGB& color);
    static void sanitizeLabel(String& label);
    static void setDefaultLabel(const String& id, String& label);
    static String lookupDefaultLabel(const String& id);

    String generateColorId() const;

    std::vector<ColorEntry> colors_;
    String activeColorId_;
    bool ready_{false};
    bool previewActive_{false};
    LightShowParams previewBackupParams_;
    CRGB previewBackupColorA_;
    CRGB previewBackupColorB_;
    // Color shifts removed - now handled in LightConduct::applyToLights()
};
