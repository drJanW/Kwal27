/**
 * @file CalendarCsv.h
 * @brief CSV file parsing for calendar data interface
 * @version 251231E
 * @date 2025-12-31
 *
 * Defines the CalendarCsvRow structure for parsed calendar CSV rows
 * and declares the parsing functions for calendar CSV files.
 * Used as an intermediate representation between raw CSV data
 * and the final CalendarEntry structures.
 */

#pragma once

#include <Arduino.h>
#include <vector>

struct CalendarCsvRow {
    uint16_t year{0};
    uint8_t month{0};
    uint8_t day{0};
    String sentence;
    uint16_t intervalMinutes{0};
    uint8_t themeBoxId{0};
    uint8_t patternId{0};
    uint8_t colorId{0};
};

bool ParseCalendarCsvRow(const std::vector<String>& columns, CalendarCsvRow& out);
