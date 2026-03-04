/**
 * @file LuxCalibration.h
 * @brief Lux calibration sample buffer and grid search fit
 * @version 260303A
 * @date 2026-03-03
 */
#pragma once

#include <Arduino.h>
#include <vector>

/// Maximum number of calibration samples in RAM
constexpr uint8_t LUX_CAL_MAX_SAMPLES = 100;

/// Single calibration sample: user pressed 👍 at this (lux, brightness) combo
struct LuxCalSample {
    float    lux;            // Raw VEML7700 reading
    float    brightness;     // Unclamped brightness from calcShiftedHi
    uint8_t  patternId;      // 1+, 0 = seed sentinel (E6)
    uint8_t  colorsId;       // 1+, 0 = seed sentinel (E6)
};

/// Grid search fit result
struct LuxFitResult {
    float luxMax;
    int8_t luxShiftLo;
    int8_t luxShiftHi;
    float luxGamma;
    float error;             // Weighted MSE
    uint8_t sampleCount;     // Samples used in fit
};

class LuxCalibration {
public:
    static LuxCalibration& instance();

    /// Add a sample to the buffer. FIFO eviction when full.
    void addSample(const LuxCalSample& sample);

    /// Number of samples currently in buffer
    uint8_t sampleCount() const { return static_cast<uint8_t>(samples_.size()); }

    /// Clear all samples from RAM (does not touch SD)
    void clearSamples();

    /// Load samples from /luxcal.csv on SD
    bool loadFromSd();

    /// Save samples to /luxcal.csv on SD (call from timer callback with isSdBusy guard)
    bool saveToSd() const;

    /// Delete /luxcal.csv from SD
    bool deleteCsv() const;

    /// Run grid search fit. Returns false if too few samples (<4).
    bool fitParams(LuxFitResult& result) const;

    /// Build JSON summary for API response
    String buildJson() const;

private:
    LuxCalibration() = default;
    std::vector<LuxCalSample> samples_;
};
