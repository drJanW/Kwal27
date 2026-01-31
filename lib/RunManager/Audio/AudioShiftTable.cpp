/**
 * @file AudioShiftTable.cpp
 * @brief Audio parameter shift storage implementation
 * @version 251231E
 * @date 2025-12-31
 *
 * Loads audio shift configuration from /audioShifts.csv on SD card.
 * Parses status conditions and percentage shifts, calculates aggregate
 * multipliers based on current context status bits.
 */

#include "AudioShiftTable.h"
#include "CsvUtils.h"
#include "SDManager.h"
#include "ContextStatus.h"
#include "Globals.h"
#include "Notify/NotifyState.h"
#include <algorithm>

namespace {
    constexpr const char* kAudioShiftPath = "/audioShifts.csv";

    bool parseStatusString(const String& s, uint8_t& out) {
        if (s == "isNight")     { out = STATUS_NIGHT; return true; }
        if (s == "isDawn")      { out = STATUS_DAWN; return true; }
        if (s == "isMorning")   { out = STATUS_MORNING; return true; }
        if (s == "isLight")     { out = STATUS_LIGHT; return true; }
        if (s == "isDay")       { out = STATUS_DAY; return true; }
        if (s == "isAfternoon") { out = STATUS_AFTERNOON; return true; }
        if (s == "isDusk")      { out = STATUS_DUSK; return true; }
        if (s == "isEvening")   { out = STATUS_EVENING; return true; }
        if (s == "isDark")      { out = STATUS_DARK; return true; }
        if (s == "isAM")        { out = STATUS_AM; return true; }
        if (s == "isPM")        { out = STATUS_PM; return true; }
        if (s == "isSpring")    { out = STATUS_SPRING; return true; }
        if (s == "isSummer")    { out = STATUS_SUMMER; return true; }
        if (s == "isAutumn")    { out = STATUS_AUTUMN; return true; }
        if (s == "isFall")      { out = STATUS_AUTUMN; return true; }
        if (s == "isWinter")    { out = STATUS_WINTER; return true; }
        if (s == "isFreezing")  { out = STATUS_FREEZING; return true; }
        if (s == "isCold")      { out = STATUS_COLD; return true; }
        if (s == "isMild")      { out = STATUS_MILD; return true; }
        if (s == "isWarm")      { out = STATUS_WARM; return true; }
        if (s == "isHot")       { out = STATUS_HOT; return true; }
        if (s == "isMonday")    { out = STATUS_MONDAY; return true; }
        if (s == "isTuesday")   { out = STATUS_TUESDAY; return true; }
        if (s == "isWednesday") { out = STATUS_WEDNESDAY; return true; }
        if (s == "isThursday")  { out = STATUS_THURSDAY; return true; }
        if (s == "isFriday")    { out = STATUS_FRIDAY; return true; }
        if (s == "isSaturday")  { out = STATUS_SATURDAY; return true; }
        if (s == "isSunday")    { out = STATUS_SUNDAY; return true; }
        if (s == "isWeekend")   { out = STATUS_WEEKEND; return true; }
        if (s == "isNewMoon")   { out = STATUS_NEW_MOON; return true; }
        if (s == "isWaxing")    { out = STATUS_WAXING; return true; }
        if (s == "isFullMoon")  { out = STATUS_FULL_MOON; return true; }
        if (s == "isWaning")    { out = STATUS_WANING; return true; }
        return false;
    }
}

AudioShiftTable& AudioShiftTable::instance() {
    static AudioShiftTable inst;
    return inst;
}

void AudioShiftTable::begin() {
    if (ready_) {
        return;
    }

    if (!NotifyState::isSdOk()) {
        PF("[AudioShiftTable] SD not ready\n");
        ready_ = true;  // Mark ready anyway, will have no shifts
        return;
    }

    if (!SDManager::fileExists(kAudioShiftPath)) {
        PF("[AudioShiftTable] %s not found\n", kAudioShiftPath);
        ready_ = true;
        return;
    }

    File file = SDManager::openFileRead(kAudioShiftPath);
    if (!file) {
        ready_ = true;
        return;
    }

    entries_.clear();

    String line;
    std::vector<String> columns;
    columns.reserve(8);

    // Column indices from header
    int colVolume = -1;
    int colFadeMs = -1;
    int colThemeBoxAdd = -1;
    bool headerLoaded = false;

    while (csv::readLine(file, line)) {
        if (line.isEmpty()) continue;

        String trimmed = line;
        trimmed.trim();
        if (trimmed.isEmpty() || trimmed.charAt(0) == '#') continue;

        csv::splitColumns(line, columns);
        if (columns.empty()) continue;

        if (!headerLoaded) {
            // Read header row
            if (columns[0] != "status") {
                PF("[AudioShiftTable] CSV header must start with 'status'\n");
                break;
            }
            for (size_t i = 1; i < columns.size(); ++i) {
                String h = columns[i];
                h.trim();
                if (h == "volume")       colVolume = static_cast<int>(i);
                else if (h == "fadeMs")  colFadeMs = static_cast<int>(i);
                else if (h == "themeBoxAdd") colThemeBoxAdd = static_cast<int>(i);
            }
            headerLoaded = true;
            continue;
        }

        // Data row
        String status = columns[0];
        status.trim();
        uint8_t statusId;
        if (!parseStatusString(status, statusId)) {
            continue;
        }

        AudioShiftEntry entry;
        entry.statusBit = 1ULL << statusId;
        entry.shifts[AUDIO_VOLUME] = 0.0f;
        entry.shifts[AUDIO_FADE_MS] = 0.0f;
        entry.themeBoxAdd = 0;

        if (colVolume >= 0 && colVolume < static_cast<int>(columns.size())) {
            entry.shifts[AUDIO_VOLUME] = columns[colVolume].toFloat();
        }
        if (colFadeMs >= 0 && colFadeMs < static_cast<int>(columns.size())) {
            entry.shifts[AUDIO_FADE_MS] = columns[colFadeMs].toFloat();
        }
        if (colThemeBoxAdd >= 0 && colThemeBoxAdd < static_cast<int>(columns.size())) {
            entry.themeBoxAdd = static_cast<uint8_t>(columns[colThemeBoxAdd].toInt());
        }

        // Only store if there's any shift or themeBoxAdd
        bool hasShift = (entry.shifts[AUDIO_VOLUME] != 0.0f) ||
                        (entry.shifts[AUDIO_FADE_MS] != 0.0f) ||
                        (entry.themeBoxAdd != 0);
        if (hasShift) {
            entries_.push_back(entry);
        }
    }

    SDManager::closeFile(file);
    ready_ = true;

    PF("[AudioShiftTable] Loaded %d audio shift entries\n", entries_.size());
}

void AudioShiftTable::computeMultipliers(uint64_t statusBits, float outMults[]) const {
    // Initialize to 1.0 (no change)
    for (int i = 0; i < AUDIO_PARAM_COUNT; i++) {
        outMults[i] = 1.0f;
    }

    // Multiply in all active shifts
    for (const auto& entry : entries_) {
        if (statusBits & entry.statusBit) {
            for (int i = 0; i < AUDIO_PARAM_COUNT; i++) {
                outMults[i] *= (1.0f + entry.shifts[i] / 100.0f);
            }
        }
    }
}

std::vector<uint8_t> AudioShiftTable::getThemeBoxAdditions(uint64_t statusBits) const {
    std::vector<uint8_t> result;
    for (const auto& entry : entries_) {
        if ((statusBits & entry.statusBit) && entry.themeBoxAdd != 0) {
            // Avoid duplicates
            if (std::find(result.begin(), result.end(), entry.themeBoxAdd) == result.end()) {
                result.push_back(entry.themeBoxAdd);
            }
        }
    }
    return result;
}

float AudioShiftTable::getEffectiveVolume(uint64_t statusBits) const {
    float mults[AUDIO_PARAM_COUNT];
    computeMultipliers(statusBits, mults);
    float vol = kBaseVolume * mults[AUDIO_VOLUME];
    // Clamp to 0.0 minimum (no negative volume)
    return (vol < 0.0f) ? 0.0f : vol;
}

uint16_t AudioShiftTable::getEffectiveFadeMs(uint64_t statusBits) const {
    float mults[AUDIO_PARAM_COUNT];
    computeMultipliers(statusBits, mults);
    float fadeMs = static_cast<float>(Globals::baseFadeMs) * mults[AUDIO_FADE_MS];
    // Clamp to reasonable range
    if (fadeMs < 0.0f) fadeMs = 0.0f;
    if (fadeMs > 60000.0f) fadeMs = 60000.0f;
    return static_cast<uint16_t>(fadeMs);
}
