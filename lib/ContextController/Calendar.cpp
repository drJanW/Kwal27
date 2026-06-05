/**
 * @file Calendar.cpp
 * @brief Calendar day structure and parsing implementation
 * @version 260605C
 * @date 2026-06-05
 */
#include <Arduino.h>
#include "Calendar.h"
#include "Globals.h"
#include "SDController.h"
#include "SdPathUtils.h"

#include <SD.h>
#include <vector>
#include <ctype.h>

#include "CsvUtils.h"
#include "CalendarCsv.h"

namespace {

constexpr const char* calendarFile        = "calendar.csv";
constexpr const char* themeBoxCsv         = "theme_boxes.csv";

using SdPathUtils::buildUploadTarget;
using SdPathUtils::sanitizeSdFilename;
using SdPathUtils::sanitizeSdPath;

bool parseUint8Strict(const String& value, uint8_t& out) {
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

CalendarSelector calendarSelector;

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

	data_ = CalendarData{};
	hasData_ = false;
	ready_ = true;
	return true;
}

bool CalendarSelector::loadToday(uint16_t year, uint8_t month, uint8_t day) {
	if (!ready_ || !fs_) {
		return false;
	}

	CalendarData calData{};
	CalendarEntry entry{};
		if (!loadCalendarRow(year, month, day, entry)) {
		hasData_ = false;
		data_ = CalendarData{};
		return false;
	}

	calData.valid = true;
	calData.day = entry;

	if (entry.themeBoxId != 0) {
		CalendarThemeBox box;
		if (loadThemeBox(entry.themeBoxId, box)) {
			calData.theme = box;
		}
	}

	data_ = calData;
	hasData_ = true;
	return true;
}

const CalendarData& CalendarSelector::calendarData() const {
	return data_;
}

bool CalendarSelector::hasCalendarData() const {
	return hasData_ && data_.valid;
}

bool CalendarSelector::isReady() const {
	return ready_ && fs_ != nullptr;
}

void CalendarSelector::clear() {
	data_ = CalendarData{};
	hasData_ = false;
}

bool CalendarSelector::loadCalendarRow(uint16_t year, uint8_t month, uint8_t day, CalendarEntry& out) {
	// Note: caller manages SD busy lock
	const String csvPath = pathFor(calendarFile);
	File file = fs_->open(csvPath.c_str(), FILE_READ);
	if (!file) {
		PF("[CalendarSelector] Failed to open %s\n", csvPath.c_str());
		return false;
	}

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
		if (row.year != year || row.month != month || row.day != day) {
			continue;
		}

		out.valid = true;
		out.year = row.year;
		out.month = row.month;
		out.day = row.day;
		out.ttsSentence = row.sentence;
		out.ttsIntervalMinutes = row.intervalMinutes;
		out.themeBoxId = row.themeBoxId;
		out.patternId = row.patternId;
		out.colorId = row.colorId;
		out.note = String();
		file.close();
		return true;
	}

	file.close();
	return false;
}

static void incrementDay(uint16_t& y, uint8_t& m, uint8_t& d) {
    static const uint8_t dim[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    uint8_t maxD = dim[m - 1];
    if (m == 2 && (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0))) maxD = 29;
    if (++d > maxD) { d = 1; if (++m > 12) { m = 1; y++; } }
}

static int32_t toJulian(uint16_t y, uint8_t m, uint8_t d) {
    int a = (14 - m) / 12;
    int yy = y + 4800 - a;
    int mm = m + 12 * a - 3;
    return d + (153 * mm + 2) / 5 + 365 * yy + yy / 4 - yy / 100 + yy / 400 - 32045;
}

bool CalendarSelector::refreshNextEvent(uint16_t year, uint8_t month, uint8_t day) {
    nextEvent_.valid = false;
    if (!ready_ || !fs_) return false;

    const String csvPath = pathFor(calendarFile);
    File file = fs_->open(csvPath.c_str(), FILE_READ);
    if (!file) return false;

    int32_t todayJ = toJulian(year, month, day);
    int32_t bestDays = INT32_MAX;
    CalendarEntry bestEntry{};

    String line;
    std::vector<String> columns;
    columns.reserve(10);
    bool headerSkipped = false;

    while (csv::readLine(file, line)) {
        if (line.isEmpty() || line.charAt(0) == '#') continue;
        if (!headerSkipped) {
            headerSkipped = true;
            if (line.startsWith(F("year"))) continue;
        }
        csv::splitColumns(line, columns);
        CalendarCsvRow row;
        if (!ParseCalendarCsvRow(columns, row)) continue;
        int32_t eventJ = toJulian(row.year, row.month, row.day);
        int32_t diff = eventJ - todayJ;
        if (diff < 0 || diff > 730) continue;
        if (diff >= bestDays) continue;
        bestDays = diff;
        bestEntry.valid          = true;
        bestEntry.year           = row.year;
        bestEntry.month          = row.month;
        bestEntry.day            = row.day;
        bestEntry.ttsSentence    = row.sentence;
        bestEntry.themeBoxId     = row.themeBoxId;
        bestEntry.patternId      = row.patternId;
        bestEntry.colorId        = row.colorId;
    }
    file.close();

    if (!bestEntry.valid) return false;
    nextEvent_.valid         = true;
    nextEvent_.year          = bestEntry.year;
    nextEvent_.month         = bestEntry.month;
    nextEvent_.day           = bestEntry.day;
    nextEvent_.daysFromToday = static_cast<uint16_t>(bestDays);
    nextEvent_.ttsSentence   = bestEntry.ttsSentence;
    nextEvent_.themeBoxId    = bestEntry.themeBoxId;
    nextEvent_.patternId     = bestEntry.patternId;
    nextEvent_.colorId       = bestEntry.colorId;
    return true;
}

bool CalendarSelector::getNextEvent(NextEventInfo& out) const {
    if (!nextEvent_.valid) return false;
    out = nextEvent_;
    return true;
}

void CalendarSelector::clearNextEvent() {
    nextEvent_.valid = false;
}

bool CalendarSelector::findNextEvent(uint16_t year, uint8_t month, uint8_t day,
                                     uint8_t maxDays, CalendarEntry& nextOut,
                                     uint8_t& daysAhead) const {
    if (!ready_ || !fs_) return false;
    uint16_t y = year; uint8_t mo = month, d = day;
    for (uint8_t i = 1; i <= maxDays; i++) {
        incrementDay(y, mo, d);
        CalendarEntry e{};
        if (const_cast<CalendarSelector*>(this)->loadCalendarRow(y, mo, d, e)) {
            nextOut   = e;
            daysAhead = i;
            return true;
        }
    }
    return false;
}

bool CalendarSelector::loadThemeBox(uint8_t id, CalendarThemeBox& out) {
	// Note: caller manages SD busy lock
	const String csvPath = pathFor(themeBoxCsv);
	File file = fs_->open(csvPath.c_str(), FILE_READ);
	if (!file) {
		PF("[CalendarSelector] Failed to open %s\n", csvPath.c_str());
		return false;
	}

	String line;
	std::vector<String> columns;
	columns.reserve(4);
	bool headerSkipped = false;

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
		if (columns.empty()) {
			continue;
		}

		const String& rowIdStr = columns[0];
		uint8_t rowId = 0;
		if (!parseUint8Strict(rowIdStr, rowId)) {
			continue;
		}

		// columns: 0=id, 1=color (skip), 2=name, 3=entries
		const String name = (columns.size() > 2) ? columns[2] : String();
		const String entries = (columns.size() > 3) ? columns[3] : String();

		if (rowId != id) {
			continue;
		}

		out.valid = true;
		out.id = rowId;
		out.entries = entries;
		out.note = name;
		file.close();
		return true;
	}

	file.close();
	PF("[CalendarSelector] Theme box %u not found in %s\n", static_cast<unsigned>(id), csvPath.c_str());
	return false;
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
