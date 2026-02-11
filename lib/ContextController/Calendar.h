/**
 * @file Calendar.h
 * @brief Calendar day structure and parsing interface
 * @version 260202A
 $12026-02-05
 */
#pragma once

#include <Arduino.h>
#include <FS.h>

struct CalendarEntry {
	bool valid{false};
	uint16_t year{0};
	uint8_t month{0};
	uint8_t day{0};
	String iso;
	String ttsSentence;
	uint16_t ttsIntervalMinutes{0};
	uint8_t themeBoxId{0};
	uint8_t patternId{0};
	uint8_t colorId{0};
	String note;
};

struct CalendarThemeBox {
	bool valid{false};
	uint8_t id{0};
	String entries;
	String note;
};

struct CalendarData {
	bool valid{false};
	CalendarEntry day;
	CalendarThemeBox theme;
};

class CalendarSelector {
public:
	bool begin(fs::FS& sd, const char* rootPath = "/");
	bool loadToday(uint16_t year, uint8_t month, uint8_t day);
	const CalendarData& calendarData() const;
	bool hasCalendarData() const;
	bool isReady() const;
	void clear();

private:
	bool loadCalendarRow(uint16_t year, uint8_t month, uint8_t day, CalendarEntry& out);
	bool loadThemeBox(uint8_t id, CalendarThemeBox& out);

	String pathFor(const char* file) const;

	fs::FS* fs_{nullptr};
	String root_{"/"};
	CalendarData data_{};
	bool ready_{false};
	bool hasData_{false};
};

extern CalendarSelector calendarSelector;
