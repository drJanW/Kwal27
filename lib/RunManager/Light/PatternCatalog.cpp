/**
 * @file PatternCatalog.cpp
 * @brief LED pattern storage implementation
 * @version 260204A
 * @date 2026-02-04
 */
#define LOCAL_LOG_LEVEL LOG_LEVEL_INFO
#include "PatternCatalog.h"

#include <algorithm>
#include <ctype.h>

#include "CsvUtils.h"
#include "SdPathUtils.h"
#include "SDController.h"
#include "Alert/AlertState.h"

namespace {
constexpr const char* kPatternPath = "/light_patterns.csv";
constexpr const char* kActivePatternPrefix = "# active_pattern=";
constexpr size_t kActivePatternPrefixLen = sizeof("# active_pattern=") - 1;
constexpr uint8_t kSchemaVersion = 1;

// No hardcoded defaults - CSV on SD is the single source of truth

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

PatternCatalog& PatternCatalog::instance() {
    static PatternCatalog inst;
    return inst;
}

void PatternCatalog::begin() {
    if (ready_) {
        return;
    }
    // Don't init if SD isn't ready - we'll be called again later
    if (!AlertState::isSdOk()) {
        return;
    }
    patterns_.clear();
    if (!loadFromSD()) {
        PL("[PatternCatalog] No CSV found - patterns empty");
    }
    // Pick a random pattern at boot - each awakening brings new motion
    if (!patterns_.empty()) {
        size_t idx = random(0, patterns_.size());
        activePatternId_ = patterns_[idx].id;
        PF_BOOT("[PatternCatalog] Boot pattern: %s\n", activePatternId_.c_str());
    } else {
        activePatternId_.clear();
    }
    ready_ = true;
}

bool PatternCatalog::selectRandom() {
    if (patterns_.empty()) {
        return false;
    }
    size_t idx = random(0, patterns_.size());
    activePatternId_ = patterns_[idx].id;
    PF("[PatternCatalog] Random pattern: %s\n", activePatternId_.c_str());
    return true;
}

String PatternCatalog::buildJson(const char* source) const {
    // Stream JSON directly - no ArduinoJson tree needed
    String out;
    out.reserve(patterns_.size() * 250 + 100);
    
    out += F("{\"schema\":");
    out += kSchemaVersion;
    out += F(",\"source\":\"");
    out += source;
    out += '"';
    
    String effectiveId = activePatternId_;
    if (effectiveId.isEmpty() && !patterns_.empty()) {
        effectiveId = patterns_.front().id;
    }
    if (!effectiveId.isEmpty()) {
        out += F(",\"active_pattern\":\"");
        out += effectiveId;
        out += '"';
    }
    
    out += F(",\"patterns\":[");
    bool first = true;
    for (const auto& entry : patterns_) {
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
        out += F(",\"params\":{");
        out += F("\"color_cycle_sec\":");  out += entry.params.colorCycleSec;
        out += F(",\"bright_cycle_sec\":"); out += entry.params.brightCycleSec;
        out += F(",\"fade_width\":");      out += String(entry.params.fadeWidth, 3);
        out += F(",\"min_brightness\":");  out += entry.params.minBrightness;
        out += F(",\"gradient_speed\":"); out += String(entry.params.gradientSpeed, 4);
        out += F(",\"center_x\":");       out += String(entry.params.centerX, 3);
        out += F(",\"center_y\":");       out += String(entry.params.centerY, 3);
        out += F(",\"radius\":");         out += String(entry.params.radius, 3);
        out += F(",\"window_width\":");   out += entry.params.windowWidth;
        out += F(",\"radius_osc\":");     out += String(entry.params.radiusOsc, 3);
        out += F(",\"x_amp\":");          out += String(entry.params.xAmp, 3);
        out += F(",\"y_amp\":");          out += String(entry.params.yAmp, 3);
        out += F(",\"x_cycle_sec\":");    out += entry.params.xCycleSec;
        out += F(",\"y_cycle_sec\":");    out += entry.params.yCycleSec;
        out += F("}}");
    }
    out += F("]}");
    return out;
}

bool PatternCatalog::select(const String& id, String& errorMessage) {
    if (!ready_) {
        errorMessage = F("catalog not ready");
        return false;
    }
    if (id.isEmpty()) {
        activePatternId_.clear();
        PF("[PatternCatalog] Cleared active pattern to context\n");
        return true;
    }
    PatternEntry* entry = findEntry(id);
    if (!entry) {
        errorMessage = F("pattern not found");
        return false;
    }
    activePatternId_ = entry->id;
    PF("[PatternCatalog] Selected %s\n", activePatternId_.c_str());
    return true;
}

bool PatternCatalog::selectNext(String& errorMessage) {
    if (!ready_) {
        errorMessage = F("catalog not ready");
        return false;
    }
    if (patterns_.empty()) {
        errorMessage = F("no patterns");
        return false;
    }
    // Find current index
    size_t currentIdx = 0;
    for (size_t i = 0; i < patterns_.size(); ++i) {
        if (patterns_[i].id == activePatternId_) {
            currentIdx = i;
            break;
        }
    }
    // Advance to next (wrap)
    size_t nextIdx = (currentIdx + 1) % patterns_.size();
    activePatternId_ = patterns_[nextIdx].id;
    LOG_DEBUG("[PatternCatalog] Pattern next -> %s\n", activePatternId_.c_str());
    return true;
}

bool PatternCatalog::selectPrev(String& errorMessage) {
    if (!ready_) {
        errorMessage = F("catalog not ready");
        return false;
    }
    if (patterns_.empty()) {
        errorMessage = F("no patterns");
        return false;
    }
    // Find current index
    size_t currentIdx = 0;
    for (size_t i = 0; i < patterns_.size(); ++i) {
        if (patterns_[i].id == activePatternId_) {
            currentIdx = i;
            break;
        }
    }
    // Go to previous (wrap)
    size_t prevIdx = (currentIdx == 0) ? patterns_.size() - 1 : currentIdx - 1;
    activePatternId_ = patterns_[prevIdx].id;
    LOG_DEBUG("[PatternCatalog] Pattern prev -> %s\n", activePatternId_.c_str());
    return true;
}

bool PatternCatalog::update(JsonVariantConst body, String& affectedId, String& errorMessage) {
    if (!ready_) {
        errorMessage = F("catalog not ready");
        return false;
    }
    JsonObjectConst obj = body.as<JsonObjectConst>();
    if (obj.isNull()) {
        errorMessage = F("invalid payload");
        return false;
    }

    LightShowParams params;
    JsonObjectConst patternObj = obj.containsKey("pattern") ? obj["pattern"].as<JsonObjectConst>() : JsonObjectConst();
    JsonVariantConst paramsVariant = obj["params"];
    if ((paramsVariant.isNull() || paramsVariant.is<JsonArrayConst>()) && !patternObj.isNull()) {
        if (patternObj.containsKey("params")) {
            paramsVariant = patternObj["params"];
        } else {
            paramsVariant = patternObj;
        }
    }

    if (!parseParams(paramsVariant, params, errorMessage)) {
        if (errorMessage.isEmpty()) {
            errorMessage = F("params missing");
        }
        return false;
    }

    String label = obj["label"].as<String>();
    if (label.isEmpty() && !patternObj.isNull() && patternObj.containsKey("label")) {
        label = patternObj["label"].as<String>();
    }
    if (label.length() > 48) {
        label = label.substring(0, 48);
    }
    bool selectEntry = obj["select"].as<bool>();
    if (!selectEntry && !patternObj.isNull() && patternObj.containsKey("select")) {
        selectEntry = patternObj["select"].as<bool>();
    }

    auto resolveId = [&]() -> String {
        if (obj.containsKey("id")) {
            return obj["id"].as<String>();
        }
        if (obj.containsKey("pattern_id")) {
            return obj["pattern_id"].as<String>();
        }
        if (!patternObj.isNull()) {
            if (patternObj.containsKey("id")) {
                return patternObj["id"].as<String>();
            }
            if (patternObj.containsKey("pattern_id")) {
                return patternObj["pattern_id"].as<String>();
            }
        }
        return String();
    };

    const String resolvedId = resolveId();

    if (!resolvedId.isEmpty()) {
        String id = resolvedId;
        PatternEntry* existing = findEntry(id);
        if (!existing) {
            errorMessage = F("pattern not found");
            return false;
        }
        existing->params = params;
        existing->label = label;
        affectedId = existing->id;
        if (selectEntry) {
            activePatternId_ = existing->id;
        }
        PF("[PatternCatalog] Updated %s%s\n", affectedId.c_str(), selectEntry ? " (selected)" : "");
        } else {
        PatternEntry entry;
        entry.id = generateId();
        entry.label = label;
        entry.params = params;
        affectedId = entry.id;
        patterns_.push_back(entry);
        if (selectEntry || activePatternId_.isEmpty()) {
            activePatternId_ = entry.id;
        }
        PF("[PatternCatalog] Created %s%s\n", affectedId.c_str(), selectEntry ? " (selected)" : "");
    }

    if (!saveToSD()) {
        errorMessage = F("write failed");
        return false;
    }
    return true;
}

bool PatternCatalog::remove(JsonVariantConst body, String& affectedId, String& errorMessage) {
    if (!ready_) {
        errorMessage = F("catalog not ready");
        return false;
    }
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
    if (patterns_.size() <= 1) {
        errorMessage = F("cannot delete last pattern");
        return false;
    }
    auto it = std::find_if(patterns_.begin(), patterns_.end(), [&](const PatternEntry& p) { return p.id == id; });
    if (it == patterns_.end()) {
        errorMessage = F("pattern not found");
        return false;
    }
    bool wasActive = (activePatternId_ == it->id);
    patterns_.erase(it);
    if (wasActive && !patterns_.empty()) {
        activePatternId_ = patterns_.front().id;
    } else if (patterns_.empty()) {
        activePatternId_.clear();
    }
    if (!saveToSD()) {
        errorMessage = F("write failed");
        return false;
    }
    affectedId = activePatternId_;
    PF("[PatternCatalog] Removed %s, fallback=%s\n", id.c_str(), affectedId.c_str());
    return true;
}

LightShowParams PatternCatalog::getActiveParams() const {
    const PatternEntry* entry = nullptr;
    if (!activePatternId_.isEmpty()) {
        entry = findEntry(activePatternId_);
    }
    if (!entry && !patterns_.empty()) {
        entry = &patterns_.front();
    }
    // Return RAW params - shifts are applied in LightRun::applyToLights()
    return entry ? entry->params : LightShowParams();
}

// setShift(), getShift(), applyShifts() REMOVED
// Pattern shifts are now applied in LightRun::applyToLights()
// See docs/refactor_light_shifts_architecture.md for rationale

bool PatternCatalog::parseParams(JsonVariantConst src, LightShowParams& out, String& errorMessage) const {
    JsonObjectConst obj = src.as<JsonObjectConst>();
    if (obj.isNull()) {
        errorMessage = F("params invalid");
        return false;
    }
    out.colorCycleSec  = obj["color_cycle_sec"].as<uint8_t>();
    out.brightCycleSec = obj["bright_cycle_sec"].as<uint8_t>();
    out.fadeWidth      = obj["fade_width"].as<float>();
    out.minBrightness  = obj["min_brightness"].as<uint8_t>();
    out.gradientSpeed  = obj["gradient_speed"].as<float>();
    out.centerX        = obj["center_x"].as<float>();
    out.centerY        = obj["center_y"].as<float>();
    out.radius         = obj["radius"].as<float>();
    out.windowWidth    = obj["window_width"].as<int>();
    out.radiusOsc      = obj["radius_osc"].as<float>();
    out.xAmp           = obj["x_amp"].as<float>();
    out.yAmp           = obj["y_amp"].as<float>();
    out.xCycleSec      = obj["x_cycle_sec"].as<uint8_t>();
    out.yCycleSec      = obj["y_cycle_sec"].as<uint8_t>();
    return true;
}

bool PatternCatalog::getParamsForId(const String& id, LightShowParams& out) const {
    if (id.isEmpty()) {
        return false;
    }
    const PatternEntry* entry = findEntry(id);
    if (!entry) {
        return false;
    }
    out = entry->params;
    return true;
}

String PatternCatalog::getLabelForId(const String& id) const {
    if (id.isEmpty()) {
        return String();
    }
    const PatternEntry* entry = findEntry(id);
    if (!entry) {
        return String();
    }
    return entry->label;
}

bool PatternCatalog::loadFromSD() {
    if (!AlertState::isSdOk()) {
        return false;
    }
    const String csvPath = SdPathUtils::chooseCsvPath(kPatternPath);
    if (csvPath.isEmpty() || !SDController::fileExists(csvPath.c_str())) {
        return false;
    }
    File file = SDController::openFileRead(csvPath.c_str());
    if (!file) {
        return false;
    }

    patterns_.clear();
    activePatternId_.clear();

    String line;
    std::vector<String> columns;
    columns.reserve(18);
    bool headerConsumed = false;

    auto toFloat = [](const String& value) -> float {
        return value.isEmpty() ? 0.0f : value.toFloat();
    };

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
            // Skip comment lines (including # active_pattern= which is saved but not restored on boot)
            // Source tracking is in Run layer, boot always starts with source=CONTEXT
            continue;
        }
        if (!headerConsumed) {
            headerConsumed = true;
            if (trimmed.startsWith(F("light_pattern_id"))) {
                continue;
            }
        }

        csv::splitColumns(line, columns);
        if (columns.size() < 16) {
            continue;
        }

        PatternEntry entry;
        entry.id = columns[0];
        entry.label = columns[1];
        if (entry.id.isEmpty()) {
            continue;
        }

        LightShowParams params;
        params.RGB1 = CRGB::Black;
        params.RGB2 = CRGB::Black;
        params.colorCycleSec  = static_cast<uint8_t>(toFloat(columns[2]));
        params.brightCycleSec = static_cast<uint8_t>(toFloat(columns[3]));
        params.fadeWidth      = toFloat(columns[4]);
        params.minBrightness  = static_cast<uint8_t>(toFloat(columns[5]));
        params.gradientSpeed  = toFloat(columns[6]);
        params.centerX        = toFloat(columns[7]);
        params.centerY        = toFloat(columns[8]);
        params.radius         = toFloat(columns[9]);
        params.windowWidth    = columns[10].toInt();
        params.radiusOsc      = toFloat(columns[11]);
        params.xAmp           = toFloat(columns[12]);
        params.yAmp           = toFloat(columns[13]);
        params.xCycleSec      = static_cast<uint8_t>(toFloat(columns[14]));
        params.yCycleSec      = static_cast<uint8_t>(toFloat(columns[15]));

        entry.params = params;
        patterns_.push_back(entry);
    }

    SDController::closeFile(file);
    return !patterns_.empty();
}

bool PatternCatalog::saveToSD() const {
    if (!AlertState::isSdOk()) {
        return false;
    }
    SDController::deleteFile(kPatternPath);
    File file = SDController::openFileWrite(kPatternPath);
    if (!file) {
        return false;
    }

    if (!activePatternId_.isEmpty()) {
        file.print(F("# active_pattern="));
        file.println(activePatternId_);
    }

    file.println(F("light_pattern_id;light_pattern_name;color_cycle_sec;bright_cycle_sec;fade_width;min_brightness;gradient_speed;center_x;center_y;radius;window_width;radius_osc;x_amp;y_amp;x_cycle_sec;y_cycle_sec"));

    for (const auto& entry : patterns_) {
        file.print(entry.id);
        file.print(';');
        file.print(entry.label);
        file.print(';');
        file.print(entry.params.colorCycleSec);
        file.print(';');
        file.print(entry.params.brightCycleSec);
        file.print(';');
        file.print(entry.params.fadeWidth, 3);
        file.print(';');
        file.print(entry.params.minBrightness);
        file.print(';');
        file.print(entry.params.gradientSpeed, 3);
        file.print(';');
        file.print(entry.params.centerX, 3);
        file.print(';');
        file.print(entry.params.centerY, 3);
        file.print(';');
        file.print(entry.params.radius, 3);
        file.print(';');
        file.print(entry.params.windowWidth);
        file.print(';');
        file.print(entry.params.radiusOsc, 3);
        file.print(';');
        file.print(entry.params.xAmp, 3);
        file.print(';');
        file.print(entry.params.yAmp, 3);
        file.print(';');
        file.print(entry.params.xCycleSec);
        file.print(';');
        file.print(entry.params.yCycleSec);
        file.println();
    }

    SDController::closeFile(file);
    return true;
}

PatternCatalog::PatternEntry* PatternCatalog::findEntry(const String& id) {
    auto it = std::find_if(patterns_.begin(), patterns_.end(), [&](PatternEntry& p) { return p.id == id; });
    if (it == patterns_.end()) {
        return nullptr;
    }
    return &(*it);
}

const PatternCatalog::PatternEntry* PatternCatalog::findEntry(const String& id) const {
    auto it = std::find_if(patterns_.begin(), patterns_.end(), [&](const PatternEntry& p) { return p.id == id; });
    if (it == patterns_.end()) {
        return nullptr;
    }
    return &(*it);
}

String PatternCatalog::generateId() const {
    int maxIndex = 0;
    bool sawPrefixed = false;
    bool sawNumeric = false;
    for (const auto& entry : patterns_) {
        const String& id = entry.id;
        int idx = 0;
        if (id.startsWith("pattern")) {
            sawPrefixed = true;
            idx = id.substring(7).toInt();
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
    snprintf(buff, sizeof(buff), "pattern%03d", next);
    return String(buff);
}
