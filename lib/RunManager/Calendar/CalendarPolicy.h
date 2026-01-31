/**
 * @file CalendarPolicy.h
 * @brief Calendar business logic
 * @version 251231E
 * @date 2025-12-31
 *
 * Contains business logic for calendar-driven decisions. Evaluates calendar
 * data to determine sentence intervals, theme box selections, and context
 * modifications. Pure logic with no state management.
 */

#pragma once

#include <Arduino.h>

#include "Calendar.h"

namespace CalendarPolicy {

struct Decision {
  bool hasSentence{false};
  uint32_t sentenceIntervalMs{0};
  bool hasThemeBox{false};
};

void configure();
bool evaluate(const CalendarData& calData, Decision& decision);
void speakSentence(const String& phrase);
void applyThemeBox(const CalendarThemeBox& box);

} // namespace CalendarPolicy
