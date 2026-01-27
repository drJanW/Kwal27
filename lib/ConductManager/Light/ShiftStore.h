/**
 * @file ShiftStore.h
 * @brief LED parameter shift storage
 * @version 251231E
 * @date 2025-12-31
 *
 * Stores and manages color/pattern parameter shifts loaded from CSV files.
 * Shifts modify brightness, speed, and other parameters based on time-of-day
 * context status. Provides computed multipliers for current context.
 */

#pragma once

#include <Arduino.h>
#include <vector>
#include <stdint.h>
#include "ContextStatus.h"

// Compact shift entry: 3 bytes data + float multiplier
struct ColorShiftEntry {
    uint8_t statusId;    // TimeStatus enum
    uint8_t paramId;     // ColorParam enum
    float   multiplier;  // pre-computed: 1.0 + (percent/100.0)
};

struct PatternShiftEntry {
    uint8_t statusId;    // TimeStatus enum
    uint8_t paramId;     // PatternParam enum
    float   multiplier;  // pre-computed: 1.0 + (percent/100.0)
};

class ShiftStore {
public:
    static ShiftStore& instance();
    
    // Load shifts from SD card CSVs (call at boot)
    bool begin();
    bool isReady() const { return ready_; }
    
    // Compute combined multipliers for all active status flags
    // Output arrays must be COLOR_PARAM_COUNT / PAT_PARAM_COUNT sized
    void computeColorMultipliers(uint64_t activeStatusBits, float* outMultipliers) const;
    void computePatternMultipliers(uint64_t activeStatusBits, float* outMultipliers) const;
    
    // Get loaded entry counts (for debugging)
    size_t colorShiftCount() const { return colorShifts_.size(); }
    size_t patternShiftCount() const { return patternShifts_.size(); }

private:
    ShiftStore() = default;
    
    bool loadColorShiftsFromSD();
    bool loadPatternShiftsFromSD();
    
    // String-to-enum parsers
    static bool parseStatusString(const String& s, uint8_t& out);
    static bool parseColorParam(const String& s, uint8_t& out);
    static bool parsePatternParam(const String& s, uint8_t& out);
    
    std::vector<ColorShiftEntry> colorShifts_;
    std::vector<PatternShiftEntry> patternShifts_;
    bool ready_{false};
};
