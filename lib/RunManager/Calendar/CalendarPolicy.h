/**
 * @file CalendarPolicy.h
 * @brief Calendar business logic
 * @version 260131A
 * @date 2026-01-31
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
