/**
 * @file Calendar.cpp
 * @brief Calendar day structure and parsing implementation
 * @version 260216H
 * @date 2026-02-16
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

constexpr const char* kCalendarFile       = "calendar.csv";
constexpr const char* kThemeBoxCsv        = "theme_boxes.csv";

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
	const String csvPath = pathFor(kCalendarFile);
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

bool CalendarSelector::loadThemeBox(uint8_t id, CalendarThemeBox& out) {
	// Note: caller manages SD busy lock
	const String csvPath = pathFor(kThemeBoxCsv);
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
		return SdPathUtils::chooseCsvPath(sanitizedFile.c_str());
	}
	return root_ + "/" + sanitizedFile;
}
