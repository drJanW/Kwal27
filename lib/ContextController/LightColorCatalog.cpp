/**
 * @file LightColorCatalog.cpp
 * @brief Color set management from CSV implementation
 * @version 260204A
 * @date 2026-02-04
 */
#include "LightColorCatalog.h"

#include "Globals.h"
#include "SDController.h"
#include "SdPathUtils.h"

#include <vector>
#include <ctype.h>

#include "CsvUtils.h"

namespace {

constexpr const char* kLightColorsFile = "light_colors.csv";

using SdPathUtils::buildUploadTarget;
using SdPathUtils::sanitizeSdFilename;
using SdPathUtils::sanitizeSdPath;

int hexValue(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return 10 + (c - 'a');
    }
    if (c >= 'A' && c <= 'F') {
        return 10 + (c - 'A');
    }
    return -1;
}

uint8_t parseByte(const String& hex, size_t offset) {
    int hi = hexValue(hex[offset]);
    int lo = hexValue(hex[offset + 1]);
    if (hi < 0 || lo < 0) {
        return 0;
    }
    return static_cast<uint8_t>((hi << 4) | lo);
}

bool parseColorId(const String& value, uint8_t& out) {
    if (value.isEmpty()) {
        return false;
    }
    for (size_t i = 0; i < value.length(); ++i) {
        const char c = value.charAt(i);
        if (c < '0' || c > '9') {
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

bool HexToRgb(const String& hex, RgbColor& out) {
    if (hex.length() != 7 || hex.charAt(0) != '#') {
        return false;
    }
    for (size_t i = 1; i < hex.length(); ++i) {
        if (hexValue(hex[i]) < 0) {
            return false;
        }
    }
    out.r = parseByte(hex, 1);
    out.g = parseByte(hex, 3);
    out.b = parseByte(hex, 5);
    return true;
}

bool LightColorCatalog::begin(fs::FS& sd, const char* rootPath) {
    fs_ = &sd;
    const String desiredRoot = (rootPath && *rootPath) ? String(rootPath) : String("/");
    const String sanitized = sanitizeSdPath(desiredRoot);
    if (sanitized.isEmpty()) {
        PF("[LightColorCatalog] Invalid root '%s', falling back to '/'\n", desiredRoot.c_str());
        root_ = "/";
    } else {
        root_ = sanitized;
    }

    clear();
    loaded_ = load();
    return loaded_;
}

bool LightColorCatalog::ready() const {
    return loaded_ && fs_ != nullptr;
}

const LightColor* LightColorCatalog::find(uint8_t id) const {
    if (!ready() || id == 0) {
        return nullptr;
    }
    for (const auto& color : colors_) {
        if (color.id == id) {
            return &color;
        }
    }
    return nullptr;
}

const LightColor* LightColorCatalog::active() const {
    if (!ready()) {
        return nullptr;
    }
    if (activeColorId_ != 0) {
        const LightColor* color = find(activeColorId_);
        if (color) {
            return color;
        }
    }
    if (!colors_.empty()) {
        return &colors_.front();
    }
    return nullptr;
}

void LightColorCatalog::clear() {
    colors_.clear();
    loaded_ = false;
    activeColorId_ = 0;
}

bool LightColorCatalog::load() {
    if (!fs_) {
        return false;
    }

    colors_.clear();
    activeColorId_ = 0;

    SDController::lockSD();
    const String path = pathFor(kLightColorsFile);
    File file = fs_->open(path.c_str(), FILE_READ);
    if (!file) {
        PF("[LightColorCatalog] failed to open %s\n", path.c_str());
        SDController::unlockSD();
        return false;
    }

    String line;
    std::vector<String> columns;
    columns.reserve(8);
    bool headerSkipped = false;
    size_t loaded = 0;

    while (csv::readLine(file, line)) {
        if (line.isEmpty() || line.charAt(0) == '#') {
            continue;
        }
        if (!headerSkipped) {
            headerSkipped = true;
            if (line.startsWith(F("light_colors_id"))) {
                continue;
            }
        }

        csv::splitColumns(line, columns);
        if (columns.size() < 4) {
            continue;
        }

        uint8_t id = 0;
        if (!parseColorId(columns[0], id)) {
            continue;
        }

        LightColor color;
        color.id = id;
        color.label = columns[1];
        const String rgb1 = columns[2];
        const String rgb2 = columns[3];
        if (color.label.isEmpty() || rgb1.isEmpty() || rgb2.isEmpty()) {
            continue;
        }
        if (!HexToRgb(rgb1, color.colorA) || !HexToRgb(rgb2, color.colorB)) {
            PF("[LightColorCatalog] invalid hex colors for id=%u\n", static_cast<unsigned>(color.id));
            continue;
        }
        color.valid = true;
        colors_.push_back(color);
        ++loaded;
    }

    file.close();
    SDController::unlockSD();

    if (colors_.empty()) {
        PF("[LightColorCatalog] no valid colors loaded from %s\n", path.c_str());
        return false;
    }

    PF("[LightColorCatalog] Loaded %u light colors\n", static_cast<unsigned>(loaded));
    return true;
}

String LightColorCatalog::pathFor(const char* file) const {
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
        return SdPathUtils::chooseCsvPath(sanitizedFile.c_str());
    }
    return root_ + "/" + sanitizedFile;
}