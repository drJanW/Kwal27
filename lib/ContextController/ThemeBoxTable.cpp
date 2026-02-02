/**
 * @file ThemeBoxTable.cpp
 * @brief Audio theme box table implementation
 * @version 251231E
 * @date 2025-12-31
 *
 * Implements the ThemeBoxTable class for managing audio theme boxes.
 * Theme boxes define collections of audio content organized by theme,
 * loaded from CSV files on SD card. Handles theme box lookup by ID,
 * active theme tracking, and provides the audio system with themed
 * content directories and configurations.
 */

#include <Arduino.h>
#include "ThemeBoxTable.h"

#include "Globals.h"
#include "SDController.h"
#include "SdPathUtils.h"

#include <utility>
#include <vector>
#include <ctype.h>

#include "CsvUtils.h"

namespace {

constexpr const char* kThemeBoxesFile = "theme_boxes.csv";

using SdPathUtils::buildUploadTarget;
using SdPathUtils::sanitizeSdFilename;
using SdPathUtils::sanitizeSdPath;

bool parseThemeBoxId(const String& value, uint8_t& out) {
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

bool ThemeBoxTable::begin(fs::FS& sd, const char* rootPath) {
    fs_ = &sd;
    const String desiredRoot = (rootPath && *rootPath) ? String(rootPath) : String("/");
    const String sanitized = sanitizeSdPath(desiredRoot);
    if (sanitized.isEmpty()) {
        PF("[ThemeBoxTable] Invalid root '%s', falling back to '/'\n", desiredRoot.c_str());
        root_ = "/";
    } else {
        root_ = sanitized;
    }

    clear();
    loaded_ = load();
    return loaded_;
}

bool ThemeBoxTable::ready() const {
    return loaded_ && fs_ != nullptr;
}

const ThemeBox* ThemeBoxTable::find(uint8_t id) const {
    if (!ready() || id == 0) {
        return nullptr;
    }
    for (const auto& box : boxes_) {
        if (box.id == id) {
            return &box;
        }
    }
    return nullptr;
}

const ThemeBox* ThemeBoxTable::active() const {
    if (!ready()) {
        return nullptr;
    }
    if (activeThemeBoxId_ != 0) {
        const ThemeBox* box = find(activeThemeBoxId_);
        if (box) {
            return box;
        }
    }
    if (!boxes_.empty()) {
        return &boxes_.front();
    }
    return nullptr;
}

void ThemeBoxTable::clear() {
    boxes_.clear();
    loaded_ = false;
    activeThemeBoxId_ = 0;
}

bool ThemeBoxTable::load() {
    if (!fs_) {
        return false;
    }

    boxes_.clear();
    activeThemeBoxId_ = 0;

    SDController::lockSD();
    const String path = pathFor(kThemeBoxesFile);
    File file = fs_->open(path.c_str(), FILE_READ);
    if (!file) {
        PF("[ThemeBoxTable] failed to open %s\n", path.c_str());
        SDController::unlockSD();
        return false;
    }

    auto parseEntries = [](const String& csv, std::vector<uint16_t>& out) {
        out.clear();
        int start = 0;
        const int len = csv.length();
        while (start <= len) {
            int idx = csv.indexOf(',', start);
            String token = (idx < 0) ? csv.substring(start) : csv.substring(start, idx);
            token.trim();
            if (!token.isEmpty()) {
                long value = token.toInt();
                if (value >= 0 && value <= 65535) {
                    out.push_back(static_cast<uint16_t>(value));
                }
            }
            if (idx < 0) {
                break;
            }
            start = idx + 1;
        }
    };

    String line;
    std::vector<String> columns;
    columns.reserve(4);
    bool headerSkipped = false;
    size_t loaded = 0;

    while (csv::readLine(file, line)) {
        if (line.isEmpty() || line.charAt(0) == '#') {
            continue;
        }
        if (!headerSkipped) {
            headerSkipped = true;
            if (line.startsWith(F("theme_box_id"))) {
                continue;
            }
        }

        csv::splitColumns(line, columns);
        if (columns.size() < 3) {
            continue;
        }

        uint8_t id = 0;
        if (!parseThemeBoxId(columns[0], id)) {
            continue;
        }

        ThemeBox box;
        box.id = id;
        box.name = columns[1];
        parseEntries(columns[2], box.entries);
        if (box.entries.empty()) {
            continue;
        }
        box.valid = true;
        boxes_.push_back(std::move(box));
        ++loaded;
    }

    file.close();
    SDController::unlockSD();

    if (boxes_.empty()) {
        PF("[ThemeBoxTable] no valid theme boxes loaded from %s\n", path.c_str());
        return false;
    }

    activeThemeBoxId_ = boxes_.front().id;
    PF("[ThemeBoxTable] Loaded %u theme boxes\n", static_cast<unsigned>(loaded));
    return true;
}

String ThemeBoxTable::pathFor(const char* file) const {
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
