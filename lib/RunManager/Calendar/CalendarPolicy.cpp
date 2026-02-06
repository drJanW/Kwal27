/**
 * @file CalendarPolicy.cpp
 * @brief Calendar business logic implementation
 * @version 260202A
 * @date 2026-02-02
 */
#include <Arduino.h>
#include "CalendarPolicy.h"

#include "Globals.h"
#include "Audio/AudioPolicy.h"
#include "SDController.h"
#include "TodayState.h"

#include <stdio.h>

namespace {

constexpr uint32_t kMinutesToMs = 60UL * 1000UL;
constexpr size_t   kMaxThemeDirs = MAX_THEME_DIRS;

size_t parseThemeEntries(const String& entries, uint8_t* dirs, size_t maxCount) {
  if (!dirs || maxCount == 0) {
    return 0;
  }

  size_t count = 0;
  int start = 0;
  const int len = entries.length();

  while (start < len && count < maxCount) {
    int sep = entries.indexOf(',', start);
    String token = (sep >= 0) ? entries.substring(start, sep) : entries.substring(start);
    token.trim();
    if (!token.isEmpty()) {
      long value = token.toInt();
      if (value >= 0 && value <= 255) {
        dirs[count++] = static_cast<uint8_t>(value);
      }
    }
    if (sep < 0) {
      break;
    }
    start = sep + 1;
  }

  return count;
}

} // namespace

namespace CalendarPolicy {

void configure() {
  AudioPolicy::clearThemeBox();
  PF("[CalendarPolicy] configured\n");
}

bool evaluate(const CalendarData& calData, Decision& decision) {
  decision = Decision{};
  if (!calData.valid) {
    return false;
  }

  decision.hasSentence = calData.day.ttsSentence.length() > 0;
  if (decision.hasSentence) {
    decision.sentenceIntervalMs = static_cast<uint32_t>(calData.day.ttsIntervalMinutes) * kMinutesToMs;
  }

  decision.hasThemeBox = calData.theme.valid && calData.theme.entries.length() > 0;

  return true;
}

void speakSentence(const String& phrase) {
  if (phrase.isEmpty()) {
    return;
  }
  AudioPolicy::requestSentence(phrase);
}

void applyThemeBox(const CalendarThemeBox& box) {
  // If no specific theme box, use the default (first) theme box
  if (!box.valid) {
    const ThemeBox* defaultBox = GetDefaultThemeBox();
    if (defaultBox && !defaultBox->entries.empty()) {
      // Convert to CalendarThemeBox format and recurse
      CalendarThemeBox defaultCal;
      defaultCal.valid = true;
      defaultCal.id = defaultBox->id;
      // Build entries string from vector
      String entriesStr;
      for (size_t i = 0; i < defaultBox->entries.size(); ++i) {
        if (i > 0) entriesStr += ",";
        entriesStr += String(defaultBox->entries[i]);
      }
      defaultCal.entries = entriesStr;
      PF_BOOT("[CalendarPolicy] default box %u\n", defaultBox->id);
      applyThemeBox(defaultCal);
      return;
    }
    AudioPolicy::clearThemeBox();
    return;
  }

  const String boxIdStr = String(box.id);

  uint8_t dirs[kMaxThemeDirs];
  const size_t count = parseThemeEntries(box.entries, dirs, kMaxThemeDirs);
  if (count == 0) {
    PF("[CalendarPolicy] Theme box %u has no valid directories, clearing\n", static_cast<unsigned>(box.id));
    AudioPolicy::clearThemeBox();
    return;
  }

  uint8_t filtered[kMaxThemeDirs];
  uint8_t skipped[kMaxThemeDirs];
  size_t filteredCount = 0;
  size_t skippedCount = 0;
  for (size_t i = 0; i < count; ++i) {
    DirEntry entry{};
    // Only keep directories that still have indexed fragment files.
    if (SDController::readDirEntry(dirs[i], &entry) && entry.fileCount > 0) {
      filtered[filteredCount++] = dirs[i];
    } else {
      skipped[skippedCount++] = dirs[i];
    }
  }
  if (skippedCount > 0) {
    PF_BOOT("[CalendarPolicy] Box %u: skipped %u empty\n",
       static_cast<unsigned>(box.id), static_cast<unsigned>(skippedCount));
  }

  if (filteredCount == 0) {
    PF("[CalendarPolicy] Theme box %u has no populated directories, clearing\n", static_cast<unsigned>(box.id));
    AudioPolicy::clearThemeBox();
    return;
  }

  AudioPolicy::setThemeBox(filtered, filteredCount, boxIdStr);
  PF_BOOT("[CalendarPolicy] Box %u: %u dirs\n",
     static_cast<unsigned>(box.id), static_cast<unsigned>(filteredCount));
}

} // namespace CalendarPolicy
