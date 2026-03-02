/**
 * @file ColorsCatalog.h
 * @brief LED color palette storage
 * @version 260302D
 * @date 2026-03-02
 */
#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <FastLED.h>
#include <vector>

#include "LightController.h"

class ColorsCatalog {
public:
    static ColorsCatalog& instance();

    void begin();
    bool isReady() const;
    bool selectRandomColor();

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

    // CNF for a colors id. Returns effective value (CIE fallback if CSV=0, else CSV value).
    float cnf(const String& id) const;

    // Parse color payload from JSON (public for LightRun::previewPattern)
    static bool parseColorPayload(JsonVariantConst src, CRGB& a, CRGB& b, String& errorMessage);

private:
    ColorsCatalog() = default;

    struct ColorEntry {
        String id;
        String label;
        CRGB colorA;
        CRGB colorB;
        float cnf{0.0f};  // 0 in CSV = compute CIE and save back
    };

    /// Reference colors id for CNF normalisation (Snow White)
    static constexpr uint8_t kReferenceColorsId = 10;

    bool loadColorsFromSD();
    bool saveColorsToSD() const;
    void ensureCnf(ColorEntry& entry);

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
    // Color shifts removed - now handled in LightRun::applyToLights()
};
