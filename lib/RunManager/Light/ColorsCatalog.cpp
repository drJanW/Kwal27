/**
 * @file ColorsCatalog.cpp
 * @brief LED color palette storage implementation
 * @version 260205A
 $12026-02-10
 */
#define LOCAL_LOG_LEVEL LOG_LEVEL_INFO
#include "ColorsCatalog.h"

#include <algorithm>
#include <ctype.h>

#include "SDController.h"
#include "CsvUtils.h"
#include "SdPathUtils.h"
#include "LightController.h"
#include "PatternCatalog.h"  // For previewColors only
#include "Alert/AlertState.h"

namespace {
constexpr const char* kColorPath = "/light_colors.csv";
constexpr const char* kActiveColorPrefix = "# active_color=";
constexpr size_t kActiveColorPrefixLen = sizeof("# active_color=") - 1;
constexpr uint8_t kSchemaVersion = 1;

// No hardcoded defaults - CSV on SD is the single source of truth

CRGB toCRGB(uint32_t value) {
    return CRGB(static_cast<uint8_t>((value >> 16) & 0xFF),
                static_cast<uint8_t>((value >> 8) & 0xFF),
                static_cast<uint8_t>(value & 0xFF));
}

bool isNumericId(const String& id) {
    if (id.isEmpty()) {
        return false;
    }
    for (size_t i = 0; i < id.length(); ++i) {
        if (!isdigit(static_cast<unsigned char>(id[i]))) {
            return false;
        }
    }
    return true;
}

} // namespace

ColorsCatalog& ColorsCatalog::instance() {
    static ColorsCatalog inst;
    return inst;
}

void ColorsCatalog::begin() {
    if (ready_) {
        return;
    }
    // Don't init if SD isn't ready - we'll be called again later
    if (!AlertState::isSdOk()) {
        return;
    }
    colors_.clear();
    loadColorsFromSD();  // If fails, colors_ stays empty - don't overwrite user's SD
    // Pick a random color at boot - the creature wakes in a new mood each day
    if (!colors_.empty()) {
        size_t idx = random(0, colors_.size());
        activeColorId_ = colors_[idx].id;
        PF_BOOT("[ColorsCatalog] Boot color: %s\n", activeColorId_.c_str());
    } else {
        activeColorId_.clear();
    }
    ready_ = true;
}

bool ColorsCatalog::selectRandomColor() {
    if (colors_.empty()) {
        return false;
    }
    size_t idx = random(0, colors_.size());
    activeColorId_ = colors_[idx].id;
    PF("[ColorsCatalog] Random color: %s\n", activeColorId_.c_str());
    return true;
}

bool ColorsCatalog::isReady() const {
    return ready_;
}

String ColorsCatalog::buildColorsJson(const char* source) const {
    // Stream JSON directly - no ArduinoJson tree needed
    String out;
    out.reserve(colors_.size() * 80 + 100);
    
    out += F("{\"schema\":");
    out += kSchemaVersion;
    out += F(",\"source\":\"");
    out += source;
    out += '"';
    
    String effectiveId = activeColorId_;
    if (effectiveId.isEmpty() && !colors_.empty()) {
        effectiveId = colors_.front().id;
    }
    if (!effectiveId.isEmpty()) {
        out += F(",\"active_color\":\"");
        out += effectiveId;
        out += '"';
    }
    
    out += F(",\"colors\":[");
    char buff[8];
    bool first = true;
    for (const auto& entry : colors_) {
        if (!first) out += ',';
        first = false;
        
        out += F("{\"id\":\"");
        out += entry.id;
        out += '"';
        if (!entry.label.isEmpty()) {
            out += F(",\"label\":\"");
            out += entry.label;
            out += '"';
        }
        snprintf(buff, sizeof(buff), "#%02X%02X%02X", entry.colorA.r, entry.colorA.g, entry.colorA.b);
        out += F(",\"colorA_hex\":\"");
        out += buff;
        out += '"';
        snprintf(buff, sizeof(buff), "#%02X%02X%02X", entry.colorB.r, entry.colorB.g, entry.colorB.b);
        out += F(",\"colorB_hex\":\"");
        out += buff;
        out += F("\"}");
    }
    out += F("]}");
    return out;
}

bool ColorsCatalog::getColorById(const String& id, String& label, CRGB& colorA, CRGB& colorB) const {
    const ColorEntry* entry = findColor(id);
    if (!entry) {
        return false;
    }
    label = entry->label;
    colorA = entry->colorA;
    colorB = entry->colorB;
    return true;
}

String ColorsCatalog::getLabelForId(const String& id) const {
    if (id.isEmpty()) {
        return String();
    }
    const ColorEntry* entry = findColor(id);
    if (!entry) {
        return String();
    }
    return entry->label;
}

void ColorsCatalog::getActiveColors(CRGB& colorA, CRGB& colorB) const {
    const ColorEntry* color = nullptr;
    if (!activeColorId_.isEmpty()) {
        color = findColor(activeColorId_);
    }
    if (!color && !colors_.empty()) {
        color = &colors_.front();
    }
    if (color) {
        colorA = color->colorA;
        colorB = color->colorB;
    } else {
        // Inline fallback - no array lookup
        colorA = CRGB(0xFF, 0x7F, 0x00);  // Orange
        colorB = CRGB(0x55, 0x22, 0x00);  // Dark orange
    }
}

bool ColorsCatalog::selectColor(const String& id, String& errorMessage) {
    if (!ready_) {
        errorMessage = F("catalog not ready");
        PF("[ColorsCatalog] selectColor rejected: catalog not ready\n");
        return false;
    }
    if (id.isEmpty()) {
        activeColorId_ = String();
        PF("[ColorsCatalog] Color cleared to context\n");
        return true;
    }
    ColorEntry* entry = findColor(id);
    if (!entry) {
        errorMessage = F("color not found");
        PF("[ColorsCatalog] selectColor unknown id='%s'\n", id.c_str());
        return false;
    }
    activeColorId_ = entry->id;
    PF("[ColorsCatalog] Color select %s\n", activeColorId_.c_str());
    return true;
}

bool ColorsCatalog::selectNextColor(String& errorMessage) {
    if (!ready_) {
        errorMessage = F("catalog not ready");
        return false;
    }
    if (colors_.empty()) {
        errorMessage = F("no colors");
        return false;
    }
    // Find current index
    size_t currentIdx = 0;
    for (size_t i = 0; i < colors_.size(); ++i) {
        if (colors_[i].id == activeColorId_) {
            currentIdx = i;
            break;
        }
    }
    // Advance to next (wrap)
    size_t nextIdx = (currentIdx + 1) % colors_.size();
    activeColorId_ = colors_[nextIdx].id;
    LOG_DEBUG("[ColorsCatalog] Color next -> %s\n", activeColorId_.c_str());
    return true;
}

bool ColorsCatalog::selectPrevColor(String& errorMessage) {
    if (!ready_) {
        errorMessage = F("catalog not ready");
        return false;
    }
    if (colors_.empty()) {
        errorMessage = F("no colors");
        return false;
    }
    // Find current index
    size_t currentIdx = 0;
    for (size_t i = 0; i < colors_.size(); ++i) {
        if (colors_[i].id == activeColorId_) {
            currentIdx = i;
            break;
        }
    }
    // Go to previous (wrap)
    size_t prevIdx = (currentIdx == 0) ? colors_.size() - 1 : currentIdx - 1;
    activeColorId_ = colors_[prevIdx].id;
    LOG_DEBUG("[ColorsCatalog] Color prev -> %s\n", activeColorId_.c_str());
    return true;
}

bool ColorsCatalog::updateColor(JsonVariantConst body, String& affectedId, String& errorMessage) {
    JsonObjectConst obj = body.as<JsonObjectConst>();
    if (obj.isNull()) {
        errorMessage = F("invalid payload");
        const bool isObject = body.is<JsonObject>();
        PF("[ColorsCatalog] updateColor reject: JsonObjectConst null (isObject=%d isNull=%d)\n",
           isObject ? 1 : 0,
           body.isNull() ? 1 : 0);
        return false;
    }
    String rawBody;
    serializeJson(body, rawBody);
    PF("[ColorsCatalog] updateColor payload=%s\n", rawBody.c_str());
    CRGB colorA;
    CRGB colorB;
    JsonVariantConst colorVariant = obj["color"];
    if (colorVariant.isNull()) {
        colorVariant = obj;
    }
    if (!parseColorPayload(colorVariant, colorA, colorB, errorMessage)) {
        return false;
    }
    JsonObjectConst colorObj = colorVariant.as<JsonObjectConst>();
    String label = obj["label"].as<String>();
    if (label.isEmpty() && !colorObj.isNull() && colorObj.containsKey("label")) {
        label = colorObj["label"].as<String>();
    }
    const bool labelKeyPresent = obj.containsKey("label") || (!colorObj.isNull() && colorObj.containsKey("label"));
    if (label.length() > 48) {
        label = label.substring(0, 48);
    }
    sanitizeLabel(label);
    // Label guard disabled: allow updates without label (legacy clients)
    // if (!labelKeyPresent || label.isEmpty()) {
    //     errorMessage = F("label required");
    //     const String logId = obj["id"].as<String>();
    //     PF("[ColorsCatalog] updateColor reject: missing label for id=%s\n", logId.c_str());
    //     return false;
    // }
    bool select = obj["select"].as<bool>();

    auto resolveId = [&]() -> String {
        if (obj.containsKey("id")) {
            return obj["id"].as<String>();
        }
        if (obj.containsKey("color_id")) {
            return obj["color_id"].as<String>();
        }
        if (!colorObj.isNull() && colorObj.containsKey("id")) {
            return colorObj["id"].as<String>();
        }
        return String();
    };

    const String resolvedId = resolveId();

    if (!resolvedId.isEmpty()) {
        String id = resolvedId;
        ColorEntry* existing = findColor(id);
        if (!existing) {
            errorMessage = F("color not found");
            return false;
        }
        existing->colorA = colorA;
        existing->colorB = colorB;
        existing->label = label;
        setDefaultLabel(existing->id, existing->label);
        affectedId = existing->id;
        if (select) {
            activeColorId_ = existing->id;
        }
    } else {
        ColorEntry entry;
        entry.id = generateColorId();
        entry.label = label;
        setDefaultLabel(entry.id, entry.label);
        entry.colorA = colorA;
        entry.colorB = colorB;
        colors_.push_back(entry);
        affectedId = entry.id;
        if (select || activeColorId_.isEmpty()) {
            activeColorId_ = entry.id;
        }
    }

    if (!saveColorsToSD()) {
        errorMessage = F("write failed");
        return false;
    }
    return true;
}

bool ColorsCatalog::deleteColorSet(JsonVariantConst body, String& affectedId, String& errorMessage) {
    JsonObjectConst obj = body.as<JsonObjectConst>();
    if (obj.isNull()) {
        errorMessage = F("invalid payload");
        return false;
    }
    String id = obj["id"].as<String>();
    if (id.isEmpty()) {
        errorMessage = F("id required");
        return false;
    }
    if (colors_.size() <= 1) {
        errorMessage = F("cannot delete last color set");
        return false;
    }
    auto it = std::find_if(colors_.begin(), colors_.end(), [&](const ColorEntry& c){ return c.id == id; });
    if (it == colors_.end()) {
        errorMessage = F("color not found");
        return false;
    }
    bool wasActive = (activeColorId_ == it->id);
    colors_.erase(it);
    if (wasActive && !colors_.empty()) {
        activeColorId_ = colors_.front().id;
    } else if (colors_.empty()) {
        activeColorId_.clear();
    }
    if (!saveColorsToSD()) {
        errorMessage = F("write failed");
        return false;
    }
    affectedId = activeColorId_;
    return true;
}

bool ColorsCatalog::previewColors(JsonVariantConst body, String& errorMessage)
{
    PatternCatalog& patternCatalog = PatternCatalog::instance();
    if (!patternCatalog.isReady())
    {
        patternCatalog.begin();
    }

    JsonObjectConst obj = body.as<JsonObjectConst>();
    if (obj.isNull())
    {
        errorMessage = F("invalid payload");
        PF("[ColorsCatalog] previewColors reject: body not object\n");
        return false;
    }

    JsonVariantConst colorVariant = obj["color"];
    if (colorVariant.isNull()) {
        colorVariant = body;
    }
    CRGB colorA;
    CRGB colorB;
    if (!parseColorPayload(colorVariant, colorA, colorB, errorMessage))
    {
        PF("[ColorsCatalog] previewColors reject: color read failed: %s\n",
           errorMessage.isEmpty() ? "<no message>" : errorMessage.c_str());
        return false;
    }

    LightShowParams params = patternCatalog.getActiveParams();

    String colorJson;
    serializeJson(colorVariant, colorJson);
    const char *colorId = obj["color_id"] | obj["id"] | "";
    PF("[ColorsCatalog] previewColors request colorId='%s' color=%s\n",
       colorId,
       colorJson.c_str());

    previewBackupParams_ = params;
    previewBackupColorA_ = colorA;
    previewBackupColorB_ = colorB;
    params.RGB1 = colorA;
    params.RGB2 = colorB;
    PlayLightShow(params);
    previewActive_ = true;
    PF("[ColorsCatalog] previewColors applied\n");
    return true;
}

bool ColorsCatalog::loadColorsFromSD() {
    if (!AlertState::isSdOk()) {
        return false;
    }
    const String csvPath = SdPathUtils::chooseCsvPath(kColorPath);
    if (csvPath.isEmpty() || !SDController::fileExists(csvPath.c_str())) {
        return false;
    }
    File file = SDController::openFileRead(csvPath.c_str());
    if (!file) {
        return false;
    }

    colors_.clear();
    activeColorId_.clear();

    String line;
    std::vector<String> columns;
    columns.reserve(8);
    bool headerConsumed = false;

    while (csv::readLine(file, line)) {
        if (line.isEmpty()) {
            continue;
        }
        String trimmed = line;
        trimmed.trim();
        if (trimmed.isEmpty()) {
            continue;
        }
        if (trimmed.charAt(0) == '#') {
            // Skip comment lines (including # active_color= which is saved but not restored on boot)
            // Source tracking is in Run layer, boot always starts with source=CONTEXT
            continue;
        }
        if (!headerConsumed) {
            headerConsumed = true;
            if (trimmed.startsWith(F("light_colors_id"))) {
                continue;
            }
        }

        csv::splitColumns(line, columns);
        if (columns.size() < 4) {
            continue;
        }

        ColorEntry entry;
        entry.id = columns[0];
        entry.label = columns[1];
        sanitizeLabel(entry.label);
        setDefaultLabel(entry.id, entry.label);
        const String rgb1 = columns[2];
        const String rgb2 = columns[3];
        if (entry.id.isEmpty() || rgb1.isEmpty() || rgb2.isEmpty()) {
            continue;
        }
        if (!parseHexColor(rgb1, entry.colorA) || !parseHexColor(rgb2, entry.colorB)) {
            PF("[ColorsCatalog] invalid hex in CSV id=%s\n", entry.id.c_str());
            continue;
        }
        colors_.push_back(entry);
    }

    SDController::closeFile(file);
    return !colors_.empty();
}

bool ColorsCatalog::saveColorsToSD() const {
    if (!AlertState::isSdOk()) {
        return false;
    }
    SDController::deleteFile(kColorPath);
    File file = SDController::openFileWrite(kColorPath);
    if (!file) {
        return false;
    }

    if (!activeColorId_.isEmpty()) {
        file.print(F("# active_color="));
        file.println(activeColorId_);
    }

    file.println(F("light_colors_id;light_colors_name;rgb1_hex;rgb2_hex"));
    for (const auto& entry : colors_) {
        file.print(entry.id);
        file.print(';');
        file.print(entry.label);
        file.print(';');
        char buff[7];
        snprintf(buff, sizeof(buff), "%02X%02X%02X", entry.colorA.r, entry.colorA.g, entry.colorA.b);
        file.print('#');
        file.print(buff);
        file.print(';');
        snprintf(buff, sizeof(buff), "%02X%02X%02X", entry.colorB.r, entry.colorB.g, entry.colorB.b);
        file.print('#');
        file.print(buff);
        file.println();
    }

    SDController::closeFile(file);
    return true;
}

const ColorsCatalog::ColorEntry* ColorsCatalog::findColor(const String& id) const {
    auto it = std::find_if(colors_.begin(), colors_.end(), [&](const ColorEntry& e){ return e.id == id; });
    if (it == colors_.end()) {
        return nullptr;
    }
    return &(*it);
}

ColorsCatalog::ColorEntry* ColorsCatalog::findColor(const String& id) {
    auto it = std::find_if(colors_.begin(), colors_.end(), [&](ColorEntry& e){ return e.id == id; });
    if (it == colors_.end()) {
        return nullptr;
    }
    return &(*it);
}

bool ColorsCatalog::parseHexColor(const String& hex, CRGB& color) {
    if (hex.length() != 7 || hex.charAt(0) != '#') {
        return false;
    }
    long value = strtol(hex.c_str() + 1, nullptr, 16);
    color.r = static_cast<uint8_t>((value >> 16) & 0xFF);
    color.g = static_cast<uint8_t>((value >> 8) & 0xFF);
    color.b = static_cast<uint8_t>(value & 0xFF);
    return true;
}

void ColorsCatalog::sanitizeLabel(String& label) {
    label.trim();
    if (label.equalsIgnoreCase(F("null"))) {
        label.clear();
    }
}

void ColorsCatalog::setDefaultLabel(const String& id, String& label) {
    label.trim();
    if (!label.isEmpty()) {
        return;
    }
    const String fallback = lookupDefaultLabel(id);
    if (!fallback.isEmpty()) {
        label = fallback;
    } else if (!id.isEmpty()) {
        label = id;
    }
}

String ColorsCatalog::lookupDefaultLabel(const String& id) {
    // No hardcoded defaults - return empty, caller will use id as label
    return String();
}

bool ColorsCatalog::parseColorPayload(JsonVariantConst src, CRGB& a, CRGB& b, String& errorMessage) {
    JsonObjectConst obj = src.as<JsonObjectConst>();
    if (obj.isNull()) {
        errorMessage = F("color invalid");
        return false;
    }

    auto readColorHex = [&](const char* hexKey,
                            const char* plainKey,
                            const char* legacyHexKey,
                            const char* legacyPlainKey) -> String {
        if (hexKey && obj.containsKey(hexKey)) {
            return obj[hexKey].as<String>();
        }
        if (plainKey && obj.containsKey(plainKey)) {
            return obj[plainKey].as<String>();
        }
        if (legacyHexKey && obj.containsKey(legacyHexKey)) {
            return obj[legacyHexKey].as<String>();
        }
        if (legacyPlainKey && obj.containsKey(legacyPlainKey)) {
            return obj[legacyPlainKey].as<String>();
        }
        return String();
    };

    const String colorAHex = readColorHex("colorA_hex", "colorA", "rgb1_hex", "primary");
    const String colorBHex = readColorHex("colorB_hex", "colorB", "rgb2_hex", "secondary");

    if (!parseHexColor(colorAHex, a) || !parseHexColor(colorBHex, b)) {
        errorMessage = F("bad color");
        return false;
    }
    return true;
}

String ColorsCatalog::generateColorId() const {
    int maxIndex = 0;
    bool sawPrefixed = false;
    bool sawNumeric = false;
    for (const auto& entry : colors_) {
        const String& id = entry.id;
        int idx = 0;
        if (id.startsWith("color")) {
            sawPrefixed = true;
            idx = id.substring(5).toInt();
        } else if (isNumericId(id)) {
            sawNumeric = true;
            idx = id.toInt();
        }
        if (idx > maxIndex) {
            maxIndex = idx;
        }
    }
    int next = maxIndex + 1;
    if (!sawPrefixed || sawNumeric) {
        return String(next);
    }
    char buff[12];
    snprintf(buff, sizeof(buff), "color%03d", next);
    return String(buff);
}

// Color shifting helper for preview
static CRGB colorShiftHSV(const CRGB &oldRGB,
                   int hueShift,        // + = vooruit op hue-cirkel, − = terug
                   int satShift,        // + = meer kleur, − = richting wit
                   int valShift,        // + = helderder, − = donkerder
                   int whiteShift)      // extra wit = saturation omlaag
{
    // Convert RGB → HSV (use approx variant to avoid unavailable helper)
    CHSV hsv=rgb2hsv_approximate(oldRGB);

    // 1. Hue shift (wrap automatisch in uint8)
    hsv.h += hueShift;

    // 2. Saturation shift
    if (satShift >= 0)
        hsv.s = qadd8(hsv.s, (uint8_t)satShift);
    else
        hsv.s = qsub8(hsv.s, (uint8_t)(-satShift));

    // 3. Value shift
    if (valShift >= 0)
        hsv.v = qadd8(hsv.v, (uint8_t)valShift);
    else
        hsv.v = qsub8(hsv.v, (uint8_t)(-valShift));

    // 4. White-shift = saturation omlaag
    if (whiteShift != 0) {
        if (whiteShift > 0)
            hsv.s = qsub8(hsv.s, (uint8_t)whiteShift);
        else
            hsv.s = qadd8(hsv.s, (uint8_t)(-whiteShift));
    }

    return CRGB(hsv);
}

CRGB colorShiftRGB(const CRGB &oldRGB,
                int redShift,
                int greenShift,
                int blueShift,
                int whiteShift)
{
    uint8_t r = oldRGB.r;
    uint8_t g = oldRGB.g;
    uint8_t b = oldRGB.b;

    // R-shift
    r = (redShift >= 0)
        ? qadd8(r, (uint8_t)redShift)
        : qsub8(r, (uint8_t)(-redShift));

    // G-shift
    g = (greenShift >= 0)
        ? qadd8(g, (uint8_t)greenShift)
        : qsub8(g, (uint8_t)(-greenShift));

    // B-shift
    b = (blueShift >= 0)
        ? qadd8(b, (uint8_t)blueShift)
        : qsub8(b, (uint8_t)(-blueShift));

    // White-shift = tegelijk alle kanalen
    if (whiteShift != 0) {
        if (whiteShift > 0) {
            uint8_t w = (uint8_t)whiteShift;
            r = qadd8(r, w);
            g = qadd8(g, w);
            b = qadd8(b, w);
        } else {
            uint8_t w = (uint8_t)(-whiteShift);
            r = qsub8(r, w);
            g = qsub8(g, w);
            b = qsub8(b, w);
        }
    }

    return CRGB(r, g, b);
}

