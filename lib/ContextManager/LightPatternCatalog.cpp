/**
 * @file LightPatternCatalog.cpp
 * @brief Pattern definitions from CSV implementation
 * @version 251231E
 * @date 2025-12-31
 *
 * Implements the LightPatternCatalog class for managing light pattern definitions.
 * Loads pattern configurations from CSV files on SD card, including pattern
 * names, animation types, timing parameters, and associated settings.
 * Provides lookup by pattern ID and active pattern tracking.
 */

#include "LightPatternCatalog.h"

#include "Globals.h"
#include "SDController.h"
#include "SdPathUtils.h"

#include <vector>
#include <ctype.h>

#include "CsvUtils.h"

namespace {

constexpr const char* kLightPatternsFile = "light_patterns.csv";

using SdPathUtils::buildUploadTarget;
using SdPathUtils::sanitizeSdFilename;
using SdPathUtils::sanitizeSdPath;

bool parsePatternId(const String& value, uint8_t& out) {
    if (value.isEmpty()) {
        return false;
    }
    for (size_t i = 0; i < value.length(); ++i) {
        if (!isdigit(static_cast<unsigned char>(value.charAt(i)))) {
            return false;
        }
    }
    const long parsed = value.toInt();
    if (parsed <= 0 || parsed > 255) {
        return false;
    }
    out = static_cast<uint8_t>(parsed);
    return true;
}

} // namespace

bool LightPatternCatalog::begin(fs::FS& sd, const char* rootPath) {
    fs_ = &sd;
    const String desiredRoot = (rootPath && *rootPath) ? String(rootPath) : String("/");
    const String sanitized = sanitizeSdPath(desiredRoot);
    if (sanitized.isEmpty()) {
        PF("[LightPatternCatalog] Invalid root '%s', falling back to '/'\n", desiredRoot.c_str());
        root_ = "/";
    } else {
        root_ = sanitized;
    }

    clear();
    loaded_ = load();
    return loaded_;
}

bool LightPatternCatalog::ready() const {
    return loaded_ && fs_ != nullptr;
}

const LightPattern* LightPatternCatalog::find(uint8_t id) const {
    if (!ready() || id == 0) {
        return nullptr;
    }
    for (const auto& pattern : patterns_) {
        if (pattern.id == id) {
            return &pattern;
        }
    }
    return nullptr;
}

const LightPattern* LightPatternCatalog::active() const {
    if (!ready()) {
        return nullptr;
    }
    if (activePatternId_ != 0) {
        const LightPattern* pattern = find(activePatternId_);
        if (pattern) {
            return pattern;
        }
    }
    if (!patterns_.empty()) {
        return &patterns_.front();
    }
    return nullptr;
}

void LightPatternCatalog::clear() {
    patterns_.clear();
    loaded_ = false;
    activePatternId_ = 0;
}

bool LightPatternCatalog::load() {
    if (!fs_) {
        return false;
    }

    patterns_.clear();
    activePatternId_ = 0;

    SDController::lockSD();
    const String path = pathFor(kLightPatternsFile);
    File file = fs_->open(path.c_str(), FILE_READ);
    if (!file) {
        PF("[LightPatternCatalog] failed to open %s\n", path.c_str());
        SDController::unlockSD();
        return false;
    }

    String line;
    std::vector<String> columns;
    columns.reserve(18);
    bool headerSkipped = false;
    size_t loaded = 0;

    auto toFloat = [](const String& value) -> float {
        return value.isEmpty() ? 0.0f : value.toFloat();
    };

    while (csv::readLine(file, line)) {
        if (line.isEmpty() || line.charAt(0) == '#') {
            continue;
        }
        if (!headerSkipped) {
            headerSkipped = true;
            if (line.startsWith(F("light_pattern_id"))) {
                continue;
            }
        }

        csv::splitColumns(line, columns);
        if (columns.size() < 16) {
            continue;
        }

        uint8_t id = 0;
        if (!parsePatternId(columns[0], id)) {
            continue;
        }

        LightPattern pattern;
        pattern.id = id;
        pattern.label = columns[1];

        pattern.color_cycle_sec  = toFloat(columns[2]);
        pattern.bright_cycle_sec = toFloat(columns[3]);
        pattern.fade_width       = toFloat(columns[4]);
        pattern.min_brightness   = toFloat(columns[5]);
        pattern.gradient_speed   = toFloat(columns[6]);
        pattern.center_x         = toFloat(columns[7]);
        pattern.center_y         = toFloat(columns[8]);
        pattern.radius           = toFloat(columns[9]);
        pattern.window_width     = toFloat(columns[10]);
        pattern.radius_osc       = toFloat(columns[11]);
        pattern.x_amp            = toFloat(columns[12]);
        pattern.y_amp            = toFloat(columns[13]);
        pattern.x_cycle_sec      = toFloat(columns[14]);
        pattern.y_cycle_sec      = toFloat(columns[15]);
        pattern.valid = true;

        patterns_.push_back(pattern);
        ++loaded;
    }

    file.close();
    SDController::unlockSD();

    if (patterns_.empty()) {
        PF("[LightPatternCatalog] no valid patterns loaded from %s\n", path.c_str());
        return false;
    }

    PF("[LightPatternCatalog] Loaded %u light patterns\n", static_cast<unsigned>(loaded));
    return true;
}

String LightPatternCatalog::pathFor(const char* file) const {
    if (!file || !*file) {
        return String();
    }
    const String sanitizedFile = sanitizeSdFilename(String(file));
    if (sanitizedFile.isEmpty()) {
        return String();
    }
    String combined = buildUploadTarget(root_, sanitizedFile);
    if (!combined.isEmpty()) {
        return combined;
    }
    if (root_ == "/") {
        return String("/") + sanitizedFile;
    }
    return root_ + "/" + sanitizedFile;
}