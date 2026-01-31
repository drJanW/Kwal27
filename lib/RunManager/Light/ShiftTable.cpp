/**
 * @file ShiftTable.cpp
 * @brief LED parameter shift storage implementation
 * @version 251231E
 * @date 2025-12-31
 *
 * Loads color and pattern shifts from /colorsShifts.csv and /patternShifts.csv
 * on SD card. Parses status conditions and percentage shifts, calculates
 * aggregate multipliers based on current time-of-day status.
 */

#include <Arduino.h>
#include "ShiftTable.h"
#include "CsvUtils.h"
#include "SDManager.h"
#include "Globals.h"
#include "Alert/AlertState.h"
#include <algorithm>

namespace {
    constexpr const char* kColorShiftPath = "/colorsShifts.csv";
    constexpr const char* kPatternShiftPath = "/patternShifts.csv";
}

ShiftTable& ShiftTable::instance() {
    static ShiftTable inst;
    return inst;
}

bool ShiftTable::begin() {
    if (ready_) {
        return true;
    }
    
    bool colorOk = loadColorShiftsFromSD();
    bool patternOk = loadPatternShiftsFromSD();
    
    PF("[ShiftTable] Loaded %d color shifts, %d pattern shifts\n",
       colorShifts_.size(), patternShifts_.size());
    
    ready_ = true;  // Mark ready even if files missing (will just have no shifts)
    return colorOk || patternOk;
}

bool ShiftTable::parseStatusString(const String& s, uint8_t& out) {
    // Time-of-day flags
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
    
    // Season flags
    if (s == "isSpring")    { out = STATUS_SPRING; return true; }
    if (s == "isSummer")    { out = STATUS_SUMMER; return true; }
    if (s == "isAutumn")    { out = STATUS_AUTUMN; return true; }
    if (s == "isFall")      { out = STATUS_AUTUMN; return true; }  // Alias
    if (s == "isWinter")    { out = STATUS_WINTER; return true; }
    
    // Weather/temperature flags
    if (s == "isFreezing")  { out = STATUS_FREEZING; return true; }
    if (s == "isCold")      { out = STATUS_COLD; return true; }
    if (s == "isMild")      { out = STATUS_MILD; return true; }
    if (s == "isWarm")      { out = STATUS_WARM; return true; }
    if (s == "isHot")       { out = STATUS_HOT; return true; }
    
    // Weekday flags
    if (s == "isMonday")    { out = STATUS_MONDAY; return true; }
    if (s == "isTuesday")   { out = STATUS_TUESDAY; return true; }
    if (s == "isWednesday") { out = STATUS_WEDNESDAY; return true; }
    if (s == "isThursday")  { out = STATUS_THURSDAY; return true; }
    if (s == "isFriday")    { out = STATUS_FRIDAY; return true; }
    if (s == "isSaturday")  { out = STATUS_SATURDAY; return true; }
    if (s == "isSunday")    { out = STATUS_SUNDAY; return true; }
    if (s == "isWeekend")   { out = STATUS_WEEKEND; return true; }
    
    // Moon phase flags
    if (s == "isNewMoon")   { out = STATUS_NEW_MOON; return true; }
    if (s == "isWaxing")    { out = STATUS_WAXING; return true; }
    if (s == "isFullMoon")  { out = STATUS_FULL_MOON; return true; }
    if (s == "isWaning")    { out = STATUS_WANING; return true; }
    
    return false;
}

bool ShiftTable::parseColorParam(const String& s, uint8_t& out) {
    String key = s;
    key.trim();
    if (key.startsWith("colors.")) {
        key = key.substring(7);
    }
    if (key == "colorA.hue")        { out = COLOR_A_HUE; return true; }
    if (key == "colorA.saturation") { out = COLOR_A_SAT; return true; }
    if (key == "colorB.hue")        { out = COLOR_B_HUE; return true; }
    if (key == "colorB.saturation") { out = COLOR_B_SAT; return true; }
    if (key == "globalBrightness")  { out = GLOBAL_BRIGHTNESS; return true; }
    return false;
}

bool ShiftTable::parsePatternParam(const String& s, uint8_t& out) {
    String key = s;
    key.trim();
    if (key.startsWith("pattern.")) {
        key = key.substring(8);
    }
    if (key == "color_cycle_sec" || key == "colorCycleSec")   { out = PAT_COLOR_CYCLE; return true; }
    if (key == "bright_cycle_sec" || key == "brightCycleSec") { out = PAT_BRIGHT_CYCLE; return true; }
    if (key == "fade_width" || key == "fadeWidth")            { out = PAT_FADE_WIDTH; return true; }
    if (key == "min_brightness" || key == "minBrightness")    { out = PAT_MIN_BRIGHT; return true; }
    if (key == "gradient_speed" || key == "gradientSpeed")    { out = PAT_GRADIENT_SPEED; return true; }
    if (key == "center_x" || key == "centerX")                { out = PAT_CENTER_X; return true; }
    if (key == "center_y" || key == "centerY")                { out = PAT_CENTER_Y; return true; }
    if (key == "radius")                                       { out = PAT_RADIUS; return true; }
    if (key == "window_width" || key == "windowWidth")        { out = PAT_WINDOW_WIDTH; return true; }
    if (key == "radius_osc" || key == "radiusOsc")            { out = PAT_RADIUS_OSC; return true; }
    if (key == "x_amp" || key == "xAmp")                      { out = PAT_X_AMP; return true; }
    if (key == "y_amp" || key == "yAmp")                      { out = PAT_Y_AMP; return true; }
    if (key == "x_cycle_sec" || key == "xCycleSec")           { out = PAT_X_CYCLE; return true; }
    if (key == "y_cycle_sec" || key == "yCycleSec")           { out = PAT_Y_CYCLE; return true; }
    return false;
}

bool ShiftTable::loadColorShiftsFromSD() {
    if (!AlertState::isSdOk()) {
        PF("[ShiftTable] SD not ready for color shifts\n");
        return false;
    }
    
    if (!SDManager::fileExists(kColorShiftPath)) {
        PF("[ShiftTable] %s not found\n", kColorShiftPath);
        return false;
    }
    
    File file = SDManager::openFileRead(kColorShiftPath);
    if (!file) {
        return false;
    }
    
    colorShifts_.clear();
    
    String line;
    std::vector<String> columns;
    columns.reserve(16);
    std::vector<int8_t> columnParamIds;
    bool headerLoaded = false;
    
    while (csv::readLine(file, line)) {
        if (line.isEmpty()) continue;
        
        String trimmed = line;
        trimmed.trim();
        if (trimmed.isEmpty() || trimmed.charAt(0) == '#') continue;
        
        csv::splitColumns(line, columns);
        if (columns.empty()) continue;
        
        if (!headerLoaded) {
            if (columns[0] != "status") {
                PF("[ShiftTable] Color CSV: header must start with 'status', got '%s'\n", columns[0].c_str());
                break;
            }
            columnParamIds.assign(columns.size(), -1);
            bool anyKnownColumns = false;
            for (size_t i = 1; i < columns.size(); ++i) {
                String headerName = columns[i];
                headerName.trim();
                uint8_t paramId;
                if (parseColorParam(headerName, paramId)) {
                    columnParamIds[i] = (int8_t)paramId;
                    anyKnownColumns = true;
                } else {
                    PF("[ShiftTable] Color CSV: ignoring column '%s'\n", headerName.c_str());
                }
            }
            if (!anyKnownColumns) {
                PF("[ShiftTable] Color CSV: no recognizable parameter columns\n");
                break;
            }
            headerLoaded = true;
            continue;
        }
        
        // Wide format: status + N parameter columns
        uint8_t statusId;
        String status = columns[0];
        status.trim();
        if (!parseStatusString(status, statusId)) {
            PF("[ShiftTable] Color CSV: unknown status '%s'\n", status.c_str());
            continue;
        }
        size_t columnCount = std::min(columns.size(), columnParamIds.size());
        for (size_t i = 1; i < columnCount; ++i) {
            int8_t paramIndex = columnParamIds[i];
            if (paramIndex < 0) continue;
            float pct = columns[i].toFloat();
            if (pct == 0.0f) continue;
            ColorShiftEntry entry;
            entry.statusId = statusId;
            entry.paramId = static_cast<uint8_t>(paramIndex);
            entry.multiplier = 1.0f + (pct / 100.0f);
            colorShifts_.push_back(entry);
        }
    }
    
    SDManager::closeFile(file);
    if (!headerLoaded) {
        PF("[ShiftTable] Color CSV: header missing or invalid\n");
        colorShifts_.clear();
        return false;
    }
    return true;
}

bool ShiftTable::loadPatternShiftsFromSD() {
    if (!AlertState::isSdOk()) {
        PF("[ShiftTable] SD not ready for pattern shifts\n");
        return false;
    }
    
    if (!SDManager::fileExists(kPatternShiftPath)) {
        PF("[ShiftTable] %s not found on SD\n", kPatternShiftPath);
        return false;
    }
    
    PF("[ShiftTable] Loading %s...\n", kPatternShiftPath);
    File file = SDManager::openFileRead(kPatternShiftPath);
    if (!file) {
        return false;
    }
    
    patternShifts_.clear();
    
    String line;
    std::vector<String> columns;
    columns.reserve(16);
    std::vector<int8_t> columnParamIds;
    bool headerLoaded = false;
    
    while (csv::readLine(file, line)) {
        if (line.isEmpty()) continue;
        
        String trimmed = line;
        trimmed.trim();
        if (trimmed.isEmpty() || trimmed.charAt(0) == '#') continue;
        
        csv::splitColumns(line, columns);
        if (columns.empty()) continue;
        
        if (!headerLoaded) {
            if (columns[0] != "status") {
                PF("[ShiftTable] Pattern CSV: header must start with 'status', got '%s'\n", columns[0].c_str());
                break;
            }
            columnParamIds.assign(columns.size(), -1);
            bool anyKnownColumns = false;
            for (size_t i = 1; i < columns.size(); ++i) {
                String headerName = columns[i];
                headerName.trim();
                uint8_t paramId;
                if (parsePatternParam(headerName, paramId)) {
                    columnParamIds[i] = (int8_t)paramId;
                    anyKnownColumns = true;
                } else {
                    PF("[ShiftTable] Pattern CSV: ignoring column '%s'\n", headerName.c_str());
                }
            }
            if (!anyKnownColumns) {
                PF("[ShiftTable] Pattern CSV: no recognizable parameter columns\n");
                break;
            }
            headerLoaded = true;
            continue;
        }
        
        uint8_t statusId;
        String status = columns[0];
        status.trim();
        if (!parseStatusString(status, statusId)) {
            PF("[ShiftTable] Pattern CSV: unknown status '%s'\n", status.c_str());
            continue;
        }
        size_t columnCount = std::min(columns.size(), columnParamIds.size());
        for (size_t i = 1; i < columnCount; ++i) {
            int8_t paramIndex = columnParamIds[i];
            if (paramIndex < 0) continue;
            float pct = columns[i].toFloat();
            if (pct == 0.0f) continue;
            PatternShiftEntry entry;
            entry.statusId = statusId;
            entry.paramId = static_cast<uint8_t>(paramIndex);
            entry.multiplier = 1.0f + (pct / 100.0f);
            patternShifts_.push_back(entry);
        }
    }
    
    SDManager::closeFile(file);
    if (!headerLoaded) {
        PF("[ShiftTable] Pattern CSV: header missing or invalid\n");
        patternShifts_.clear();
        return false;
    }
    return true;
}

void ShiftTable::computeColorMultipliers(uint64_t activeStatusBits, float* outMultipliers) const {
    // Start all at 1.0 (no change)
    for (int i = 0; i < COLOR_PARAM_COUNT; i++) {
        outMultipliers[i] = 1.0f;
    }
    
    // Multiply in all active shifts
    for (const auto& entry : colorShifts_) {
        if (activeStatusBits & (1ULL << entry.statusId)) {
            outMultipliers[entry.paramId] *= entry.multiplier;
        }
    }
}

void ShiftTable::computePatternMultipliers(uint64_t activeStatusBits, float* outMultipliers) const {
    // Start all at 1.0 (no change)
    for (int i = 0; i < PAT_PARAM_COUNT; i++) {
        outMultipliers[i] = 1.0f;
    }
    
    // Multiply in all active shifts
    for (const auto& entry : patternShifts_) {
        if (activeStatusBits & (1ULL << entry.statusId)) {
            outMultipliers[entry.paramId] *= entry.multiplier;
        }
    }
}
