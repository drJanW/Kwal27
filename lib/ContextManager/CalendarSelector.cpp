/**
 * @file CalendarSelector.cpp
 * @brief Calendar data selection and lookup implementation
 * @version 251231E
 * @date 2025-12-31
 *
 * Implements the CalendarSelector class for managing calendar data.
 * Handles loading calendar entries from SD card, maintaining an in-memory
 * collection of entries, and providing lookup functionality to find
 * calendar entries by date. Supports the context system's need for
 * date-specific configuration and TTS announcements.
 */

#include <Arduino.h>
#include "CalendarSelector.h"

#include "Globals.h"
#include "SDController.h"
#include "SdPathUtils.h"
#include "PRTClock.h"

#include <ctype.h>
#include <vector>

#include "CsvUtils.h"
#include "CalendarCsv.h"

namespace {

constexpr const char* kCalendarFile = "calendar.csv";

using SdPathUtils::buildUploadTarget;
using SdPathUtils::sanitizeSdFilename;
using SdPathUtils::sanitizeSdPath;

bool resolveToday(uint16_t& year, uint8_t& month, uint8_t& day) {
    const uint16_t rawYear = prtClock.getYear();
    month = prtClock.getMonth();
    day = prtClock.getDay();
    if (rawYear == 0 || month == 0 || day == 0) {
        return false;
    }

    if (rawYear >= 1900) {
        year = rawYear;
    } else {
        year = static_cast<uint16_t>(2000 + rawYear);
    }
    return true;
}

int compareDate(uint16_t lhsYear, uint8_t lhsMonth, uint8_t lhsDay,
                uint16_t rhsYear, uint8_t rhsMonth, uint8_t rhsDay) {
    if (lhsYear != rhsYear) {
        return lhsYear < rhsYear ? -1 : 1;
    }
    if (lhsMonth != rhsMonth) {
        return lhsMonth < rhsMonth ? -1 : 1;
    }
    if (lhsDay != rhsDay) {
        return lhsDay < rhsDay ? -1 : 1;
    }
    return 0;
}

String makeIso(uint16_t year, uint8_t month, uint8_t day) {
    char buffer[12];
    snprintf(buffer, sizeof(buffer), "%04u-%02u-%02u",
             static_cast<unsigned>(year), static_cast<unsigned>(month), static_cast<unsigned>(day));
    return String(buffer);
}

} // namespace

namespace Context {

bool CalendarSelector::begin(fs::FS& sd, const char* rootPath) {
    fs_ = &sd;
    const String desiredRoot = (rootPath && *rootPath) ? String(rootPath) : String("/");
    const String sanitized = sanitizeSdPath(desiredRoot);
    if (sanitized.isEmpty()) {
        PF("[CalendarSelector] Invalid root '%s', falling back to '/'\n", desiredRoot.c_str());
        root_ = "/";
    } else {
        root_ = sanitized;
    }

    entries_.clear();
    loaded_ = load();
    return loaded_;
}

bool CalendarSelector::ready() const {
    return loaded_ && fs_ != nullptr;
}

bool CalendarSelector::findEntry(uint16_t year, uint8_t month, uint8_t day, CalendarEntry& out) const {
    if (!ready()) {
        return false;
    }

    for (const auto& entry : entries_) {
        if (entry.year == year && entry.month == month && entry.day == day) {
            out = entry;
            return true;
        }
    }
    return false;
}

void CalendarSelector::clear() {
    entries_.clear();
    loaded_ = false;
}

bool CalendarSelector::load() {
    if (!fs_) {
        return false;
    }

    SDController::lockSD();
    const String path = pathFor(kCalendarFile);
    File file = fs_->open(path.c_str(), FILE_READ);
    if (!file) {
        PF("[CalendarSelector] failed to open %s\n", path.c_str());
        SDController::unlockSD();
        return false;
    }

    uint16_t todayYear = 0;
    uint8_t todayMonth = 0;
    uint8_t todayDay = 0;
    if (!resolveToday(todayYear, todayMonth, todayDay)) {
        file.close();
        SDController::unlockSD();
        return false;
    }

    entries_.clear();

    String line;
    std::vector<String> columns;
    columns.reserve(10);
    bool headerSkipped = false;

    while (csv::readLine(file, line)) {
        if (line.isEmpty() || line.charAt(0) == '#') {
            continue;
        }
        if (!headerSkipped) {
            headerSkipped = true;
            if (line.startsWith(F("year"))) {
                continue;
            }
        }

        csv::splitColumns(line, columns);
        CalendarCsvRow row;
        if (!ParseCalendarCsvRow(columns, row)) {
            continue;
        }

        CalendarEntry entry{};
        entry.valid = true;
        entry.year = row.year;
        entry.month = row.month;
        entry.day = row.day;
        entry.iso = makeIso(row.year, row.month, row.day);
        entry.ttsSentence = row.sentence;
        entry.ttsIntervalMinutes = row.intervalMinutes;
        entry.themeBoxId = row.themeBoxId;
        entry.patternId = row.patternId;
        entry.colorId = row.colorId;

        if (entry.themeBoxId == 0) {
            PF("[CalendarSelector] entry %u-%u-%u missing theme_box_id, will use defaults\n",
               static_cast<unsigned>(entry.year), static_cast<unsigned>(entry.month), static_cast<unsigned>(entry.day));
        }
        if (entry.patternId == 0 || entry.colorId == 0) {
            PF("[CalendarSelector] entry %u-%u-%u missing pattern/color ids, will use defaults\n",
               static_cast<unsigned>(entry.year), static_cast<unsigned>(entry.month), static_cast<unsigned>(entry.day));
        }

        const int cmp = compareDate(entry.year, entry.month, entry.day,
                                    todayYear, todayMonth, todayDay);
        if (cmp < 0) {
            continue;
        }
        if (cmp > 0) {
            break;
        }

        entries_.push_back(entry);
    }

    file.close();
    SDController::unlockSD();

    if (entries_.empty()) {
        PF("[CalendarSelector] No special entries for %04u-%02u-%02u\n",
           static_cast<unsigned>(todayYear), static_cast<unsigned>(todayMonth),
           static_cast<unsigned>(todayDay));
    } else {
        PF("[CalendarSelector] Loaded %u calendar entries for %04u-%02u-%02u\n",
           static_cast<unsigned>(entries_.size()), static_cast<unsigned>(todayYear),
           static_cast<unsigned>(todayMonth), static_cast<unsigned>(todayDay));
    }
    return true;
}

String CalendarSelector::pathFor(const char* file) const {
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

} // namespace Context
