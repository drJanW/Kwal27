/**
 * @file CalendarManager.h
 * @brief Calendar data management and lookup interface
 * @version 251231E
 * @date 2025-12-31
 *
 * Defines the CalendarManager class within the Context namespace.
 * Provides interface for initializing calendar storage from SD card,
 * checking readiness, and looking up calendar entries by date.
 * Core component of the context system for date-aware behavior.
 */

#pragma once

#include <Arduino.h>
#include <FS.h>
#include <vector>

#include "ContextModels.h"

namespace Context {

class CalendarManager {
public:
	bool begin(fs::FS& sd, const char* rootPath = "/");
	bool ready() const;
	bool findEntry(uint16_t year, uint8_t month, uint8_t day, CalendarEntry& out) const;
	void clear();

private:
	bool load();
	String pathFor(const char* file) const;

	fs::FS* fs_{nullptr};
	String root_{"/"};
	bool loaded_{false};
	std::vector<CalendarEntry> entries_;
};

} // namespace Context
