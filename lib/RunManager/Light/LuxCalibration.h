/**
 * @file LuxCalibration.h
 * @brief Lux calibration data file and Gauss-Newton fit
 * @version 260319A
 * @date 2026-03-19
 */
#pragma once

#include <Arduino.h>
#include <vector>

/// Single calibration data point (seed or real sample)
struct LuxCalSample {
    float    lux;            // Raw VEML7700 reading
    float    brightness;     // Normalized brightness (context shifts divided out, webMult kept)
    float    nf;             // Normalization divisor used: colorMults[GLOBAL_BRIGHTNESS] * cnf * pnf
};

/// Gauss-Newton fit result
struct LuxFitResult {
    float brMax;             // Fitted brightness asymptote
    float luxRate;           // Fitted saturation rate
    float r2;                // Coefficient of determination (0..1)
    uint8_t sampleCount;     // Total data points used in fit
};

class LuxCalibration {
public:
    static LuxCalibration& instance();

    /// Add a real data point.
    void addSample(const LuxCalSample& sample);

    /// True when sample buffer has reached maxLuxDataPoints
    bool isFull() const;

    /// Total data points in buffer (seeds + real)
    uint8_t sampleCount() const { return static_cast<uint8_t>(samples_.size()); }

    /// Real samples added since last generateSeeds()
    uint8_t realCount() const { return realCount_; }

    /// Clear all data points from RAM (does not touch SD)
    void clearSamples();

    /// Clear real samples only, keep seeds intact
    void clearRealSamples();

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

    /// Store fit result for later accept
    void setPendingFit(const LuxFitResult& fit) { pendingFit_ = fit; hasPendingFit_ = true; }
    bool hasPendingFit() const { return hasPendingFit_; }
    const LuxFitResult& getPendingFit() const { return pendingFit_; }
    void clearPendingFit() { hasPendingFit_ = false; }

    /// Persist fitted params to globals.csv (append, last value wins)
    bool saveFittedParams(const LuxFitResult& result) const;

    /// Build JSON summary for API response
    String buildJson() const;

    /// Read-only access to sample buffer (for points endpoint)
    const std::vector<LuxCalSample>& samples() const { return samples_; }

private:
    LuxCalibration() = default;
    std::vector<LuxCalSample> samples_;
    uint8_t realCount_ = 0;
    LuxFitResult pendingFit_;
    bool hasPendingFit_ = false;
};
