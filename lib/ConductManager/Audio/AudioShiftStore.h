/**
 * @file AudioShiftStore.h
 * @brief Audio parameter shift storage
 * @version 251231E
 * @date 2025-12-31
 *
 * Stores and manages audio parameter shifts loaded from CSV configuration.
 * Shifts modify volume and fade timing based on context flags (time of day,
 * calendar events). Provides computed shift values for current context status.
 */

#pragma once

#include <Arduino.h>
#include <vector>

// Audio parameter indices for shift arrays
enum AudioParam {
    AUDIO_VOLUME = 0,
    AUDIO_FADE_MS = 1,
    AUDIO_PARAM_COUNT = 2
};

// Single shift entry parsed from CSV
struct AudioShiftEntry {
    uint64_t statusBit;                     // ContextFlags bit
    float shifts[AUDIO_PARAM_COUNT];        // percentage shifts (-100 to +inf)
    uint8_t themeBoxAdd;                    // extra ThemeBox ID (0 = none)
};

class AudioShiftStore {
public:
    static AudioShiftStore& instance();

    // Load shifts from /audioShifts.csv
    void begin();
    bool isReady() const { return ready_; }

    // Compute combined multipliers for all active statuses
    // outMults must be AUDIO_PARAM_COUNT sized
    void computeMultipliers(uint64_t statusBits, float outMults[]) const;

    // Get list of themeBoxAdd values for active statuses (non-zero only)
    std::vector<uint8_t> getThemeBoxAdditions(uint64_t statusBits) const;

    // Get effective values given current context
    float getEffectiveVolume(uint64_t statusBits) const;      // 0.0 - 1.0+
    uint16_t getEffectiveFadeMs(uint64_t statusBits) const;   // milliseconds

    // Base values (defaults when no shifts active)
    static constexpr float kBaseVolume = 1.0f;          // 100%

private:
    AudioShiftStore() = default;
    AudioShiftStore(const AudioShiftStore&) = delete;
    AudioShiftStore& operator=(const AudioShiftStore&) = delete;

    bool parseRow(const String& line);

    std::vector<AudioShiftEntry> entries_;
    bool ready_ = false;
};
