/**
 * @file AudioShiftTable.h
 * @brief Audio parameter shift storage
 * @version 260309C
 * @date 2026-03-09
 */
#pragma once

#include <Arduino.h>
#include <vector>

// Audio parameter indices for shift arrays
// Each value is a percentage shift applied multiplicatively.
// E.g. speakMin +100 in isEvening doubles the speak minimum interval at evening.
enum AudioParam {
    AUDIO_VOLUME = 0,
    AUDIO_FADE_MS = 1,
    AUDIO_SPEAK_MIN = 2,    // % shift on speakTime minimum interval
    AUDIO_SPEAK_MAX = 3,    // % shift on speakTime maximum interval
    AUDIO_FRAG_MIN = 4,     // % shift on playFragment minimum interval
    AUDIO_FRAG_MAX = 5,     // % shift on playFragment maximum interval
    AUDIO_PARAM_COUNT = 6
};

// Single shift entry parsed from CSV
struct AudioShiftEntry {
    uint64_t statusBit;                     // StatusFlags bit
    float shifts[AUDIO_PARAM_COUNT];        // percentage shifts (-100 to +inf)
    uint8_t themeBoxAdd;                    // extra ThemeBox ID (0 = none)
};

class AudioShiftTable {
public:
    static AudioShiftTable& instance();

    // Load shifts from /audioShifts.csv
    void begin();
    bool isReady() const { return ready_; }

    // Compute combined multipliers for all active statuses
    // outMults must be AUDIO_PARAM_COUNT sized
    void computeMultipliers(uint64_t statusBits, float outMults[]) const;

    // Get list of themeBoxAdd values for active statuses (non-zero only)
    std::vector<uint8_t> getThemeBoxAdditions(uint64_t statusBits) const;

    // Get effective values given current context
    float getVolumeMultiplier(uint64_t statusBits) const;     // 0.0 - 1.0+
    uint16_t getFadeMs(uint64_t statusBits) const;            // milliseconds
    float getSpeakMinMultiplier(uint64_t statusBits) const;   // 1.0 = no change
    float getSpeakMaxMultiplier(uint64_t statusBits) const;
    float getFragMinMultiplier(uint64_t statusBits) const;
    float getFragMaxMultiplier(uint64_t statusBits) const;

    // Base values (defaults when no shifts active)
    static constexpr float baseVolume = 1.0f;          // 100%

private:
    AudioShiftTable() = default;
    AudioShiftTable(const AudioShiftTable&) = delete;
    AudioShiftTable& operator=(const AudioShiftTable&) = delete;

    bool parseRow(const String& line);

    std::vector<AudioShiftEntry> entries_;
    bool ready_ = false;
};
