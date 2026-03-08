/**
 * @file LuxCalibration.h
 * @brief Lux calibration data file and grid search fit
 * @version 260308L
 * @date 2026-03-08
 */
#pragma once

#include <Arduino.h>
#include <vector>

/// Single calibration data point (seed or real sample)
struct LuxCalSample {
    float    lux;            // Raw VEML7700 reading
    float    brightness;     // Unclamped brightness from calcShiftedHi
    uint8_t  patternId;      // Pattern active during measurement
    uint8_t  colorsId;       // Colors active during measurement
};

/// Grid search fit result
struct LuxFitResult {
    float luxMax;
    int8_t luxShiftLo;
    int8_t luxShiftHi;
    float luxGamma;
    float error;             // Weighted MSE
    uint8_t sampleCount;     // Total data points used in fit
};

class LuxCalibration {
public:
    static LuxCalibration& instance();

    /// Add a data point. FIFO eviction when buffer exceeds maxLuxDataPoints.
    void addSample(const LuxCalSample& sample);

    /// Number of data points currently in buffer
    uint8_t sampleCount() const { return static_cast<uint8_t>(samples_.size()); }

    /// Clear all data points from RAM (does not touch SD)
    void clearSamples();

    /// Generate seed data points from current Globals params, store in RAM + SD
    bool generateSeeds();

    /// Load data points from /luxcal.csv on SD
    bool loadFromSd();

    /// Save all data points to /luxcal.csv on SD (call from timer callback with isSdBusy guard)
    bool saveToSd() const;

    /// Delete /luxcal.csv from SD
    bool deleteCsv() const;

    /// Run grid search fit. Returns false if insufficient data.
    bool fitParams(LuxFitResult& result) const;

    /// Persist fitted params to globals.csv (append, last value wins)
    bool saveFittedParams(const LuxFitResult& result) const;

    /// Build JSON summary for API response
    String buildJson() const;

private:
    LuxCalibration() = default;
    std::vector<LuxCalSample> samples_;
};
