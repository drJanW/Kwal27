/**
 * @file CalendarCsv.cpp
 * @brief CSV file parsing for calendar data implementation
 * @version 260202A
 * @date 2026-02-02
 */
#include "CalendarCsv.h"

#include <ctype.h>

namespace {

bool isDigits(const String& value) {
    if (value.isEmpty()) {
        return false;
    }
    for (size_t i = 0; i < value.length(); ++i) {
        if (!isdigit(static_cast<unsigned char>(value.charAt(i)))) {
            return false;
        }
    }
    return true;
}

bool parseUint8Column(const String& value, uint8_t& out) {
    if (value.isEmpty()) {
        out = 0;
        return true;
    }
    if (!isDigits(value)) {
        return false;
    }
    const long parsed = value.toInt();
    if (parsed < 0 || parsed > 255) {
        return false;
    }
    out = static_cast<uint8_t>(parsed);
    return true;
}

} // namespace

bool ParseCalendarCsvRow(const std::vector<String>& columns, CalendarCsvRow& out) {
    if (columns.size() < 8) {
        return false;
    }

    const String& yearStr = columns[0];
    const String& monthStr = columns[1];
    const String& dayStr = columns[2];
    if (!isDigits(yearStr) || !isDigits(monthStr) || !isDigits(dayStr)) {
        return false;
    }

    const uint16_t year = static_cast<uint16_t>(yearStr.toInt());
    const uint8_t month = static_cast<uint8_t>(monthStr.toInt());
    const uint8_t day = static_cast<uint8_t>(dayStr.toInt());
    if (year == 0 || month == 0 || day == 0) {
        return false;
    }

    CalendarCsvRow parsed;
    parsed.year = year;
    parsed.month = month;
    parsed.day = day;
    parsed.sentence = columns[3];
    parsed.intervalMinutes = static_cast<uint16_t>(columns[4].toInt());
    if (!parseUint8Column(columns[5], parsed.themeBoxId) ||
        !parseUint8Column(columns[6], parsed.patternId) ||
        !parseUint8Column(columns[7], parsed.colorId)) {
        return false;
    }

    out = parsed;
    return true;
}
