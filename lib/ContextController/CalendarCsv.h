/**
 * @file CalendarCsv.h
 * @brief CSV file parsing for calendar data interface
 * @version 260202A
 * @date 2026-02-02
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
