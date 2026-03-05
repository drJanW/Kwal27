/**
 * @file LuxCalibration.cpp
 * @brief Lux calibration sample buffer and grid search fit implementation
 * @version 260304H
 * @date 2026-03-04
 */
#define LOCAL_LOG_LEVEL LOG_LEVEL_INFO
#include <Arduino.h>

#include "LuxCalibration.h"
#include "Globals.h"
#include "CsvUtils.h"
#include "SDController.h"
#include "SensorController.h"
#include "Alert/AlertState.h"
#include <math.h>

namespace {
constexpr const char* kLuxCalPath = "/luxcal.csv";
constexpr const char* kCsvHeader  = "lux;brightness;patternId;colorsId";
constexpr uint8_t kMinFitSamples  = 4;
} // namespace

LuxCalibration& LuxCalibration::instance() {
    static LuxCalibration inst;
    return inst;
}

void LuxCalibration::addSample(const LuxCalSample& sample) {
    // FIFO eviction when buffer is full
    if (samples_.size() >= LUX_CAL_MAX_SAMPLES) {
        samples_.erase(samples_.begin());
    }
    samples_.push_back(sample);
    PF("[LuxCal] Sample added: lux=%.1f bri=%.1f pat=%u col=%u (n=%u)\n",
       sample.lux, sample.brightness, sample.patternId, sample.colorsId,
       static_cast<unsigned>(samples_.size()));
}

void LuxCalibration::clearSamples() {
    samples_.clear();
    PL("[LuxCal] Samples cleared");
}

bool LuxCalibration::loadFromSd() {
    if (!AlertState::isSdOk()) return false;
    if (!SDController::fileExists(kLuxCalPath)) return false;

    File file = SDController::openFileRead(kLuxCalPath);
    if (!file) return false;

    samples_.clear();
    String line;
    std::vector<String> columns;
    columns.reserve(6);
    bool headerConsumed = false;

    while (csv::readLine(file, line)) {
        String trimmed = line;
        trimmed.trim();
        if (trimmed.isEmpty() || trimmed.charAt(0) == '#') continue;

        if (!headerConsumed) {
            headerConsumed = true;
            if (trimmed.startsWith(F("lux"))) continue;
        }

        csv::splitColumns(line, columns);
        if (columns.size() < 4) continue;

        LuxCalSample s;
        s.lux        = columns[0].toFloat();
        s.brightness = columns[1].toFloat();
        s.patternId  = static_cast<uint8_t>(columns[2].toInt());
        s.colorsId   = static_cast<uint8_t>(columns[3].toInt());
        samples_.push_back(s);
    }
    SDController::closeFile(file);
    PF("[LuxCal] Loaded %u samples from SD\n", static_cast<unsigned>(samples_.size()));
    return true;
}

bool LuxCalibration::saveToSd() const {
    if (!AlertState::isSdOk()) return false;

    SDController::deleteFile(kLuxCalPath);
    File file = SDController::openFileWrite(kLuxCalPath);
    if (!file) return false;

    file.println(kCsvHeader);
    for (const auto& s : samples_) {
        file.print(s.lux, 1);
        file.print(';');
        file.print(s.brightness, 1);
        file.print(';');
        file.print(s.patternId);
        file.print(';');
        file.println(s.colorsId);
    }
    SDController::closeFile(file);
    PF("[LuxCal] Saved %u samples to SD\n", static_cast<unsigned>(samples_.size()));
    return true;
}

bool LuxCalibration::deleteCsv() const {
    if (!AlertState::isSdOk()) return false;
    SDController::deleteFile(kLuxCalPath);
    PL("[LuxCal] CSV deleted");
    return true;
}

bool LuxCalibration::fitParams(LuxFitResult& result) const {
    // Filter: exclude seed sentinels (patternId==0 or colorsId==0)
    std::vector<const LuxCalSample*> valid;
    valid.reserve(samples_.size());
    for (const auto& s : samples_) {
        if (s.patternId == 0 || s.colorsId == 0) continue;
        valid.push_back(&s);
    }

    if (valid.size() < kMinFitSamples) {
        PF("[LuxCal] FIT: too few samples (%u < %u)\n",
           static_cast<unsigned>(valid.size()), kMinFitSamples);
        return false;
    }

    // luxMax = max(all sample lux) × 1.1
    float maxLux = 0.0f;
    for (const auto* s : valid) {
        if (s->lux > maxLux) maxLux = s->lux;
    }
    float fitLuxMax = maxLux * 1.1f;
    if (fitLuxMax < 10.0f) fitLuxMax = 10.0f;  // Sanity floor

    // Grid search: 3 params
    // luxShiftLo ∈ [-30 .. 0] step 5 → 7 values
    // luxShiftHi ∈ [0 .. +30] step 5 → 7 values
    // luxGamma   ∈ [0.2 .. 0.7] step 0.1 → 6 values
    float bestError = 1e12f;
    int8_t bestShiftLo = 0;
    int8_t bestShiftHi = 0;
    float bestGamma = 0.4f;

    for (int sLo = -30; sLo <= 0; sLo += 5) {
        for (int sHi = 0; sHi <= 30; sHi += 5) {
            for (int gIdx = 2; gIdx <= 7; gIdx++) {  // 0.2 .. 0.7
                float gamma = gIdx * 0.1f;

                // Compute weighted MSE for this parameter set
                float totalError = 0.0f;
                float totalWeight = 0.0f;

                for (const auto* s : valid) {
                    float normLux = clamp(s->lux, Globals::luxMin, fitLuxMax) / fitLuxMax;
                    float luxT = powf(normLux, gamma);
                    float luxShift = sLo + (sHi - sLo) * luxT;
                    float predicted = Globals::brightnessHi * (1.0f + luxShift / 100.0f);

                    float diff = s->brightness - predicted;
                    float weight = 1.0f / (1.0f + s->lux);  // Low-lux emphasis
                    totalError += diff * diff * weight;
                    totalWeight += weight;
                }

                float mse = (totalWeight > 0.0f) ? totalError / totalWeight : 1e12f;
                if (mse < bestError) {
                    bestError = mse;
                    bestShiftLo = static_cast<int8_t>(sLo);
                    bestShiftHi = static_cast<int8_t>(sHi);
                    bestGamma = gamma;
                }
            }
        }
    }

    // Optional: second pass ±2 steps at 0.5× step size around best
    for (int sLo = bestShiftLo - 5; sLo <= bestShiftLo + 5; sLo += 2) {
        if (sLo < -40 || sLo > 0) continue;
        for (int sHi = bestShiftHi - 5; sHi <= bestShiftHi + 5; sHi += 2) {
            if (sHi < 0 || sHi > 40) continue;
            for (int gIdx = static_cast<int>(bestGamma * 20.0f) - 2;
                     gIdx <= static_cast<int>(bestGamma * 20.0f) + 2; gIdx++) {
                float gamma = gIdx * 0.05f;
                if (gamma < 0.15f || gamma > 0.8f) continue;

                float totalError = 0.0f;
                float totalWeight = 0.0f;

                for (const auto* s : valid) {
                    float normLux = clamp(s->lux, Globals::luxMin, fitLuxMax) / fitLuxMax;
                    float luxT = powf(normLux, gamma);
                    float luxShift = sLo + (sHi - sLo) * luxT;
                    float predicted = Globals::brightnessHi * (1.0f + luxShift / 100.0f);

                    float diff = s->brightness - predicted;
                    float weight = 1.0f / (1.0f + s->lux);
                    totalError += diff * diff * weight;
                    totalWeight += weight;
                }

                float mse = (totalWeight > 0.0f) ? totalError / totalWeight : 1e12f;
                if (mse < bestError) {
                    bestError = mse;
                    bestShiftLo = static_cast<int8_t>(clamp(sLo, -40, 0));
                    bestShiftHi = static_cast<int8_t>(clamp(sHi, 0, 40));
                    bestGamma = gamma;
                }
            }
        }
    }

    // Clamp outputs
    result.luxMax      = fitLuxMax;
    result.luxShiftLo  = static_cast<int8_t>(clamp(static_cast<int>(bestShiftLo), -40, 0));
    result.luxShiftHi  = static_cast<int8_t>(clamp(static_cast<int>(bestShiftHi), 0, 40));
    result.luxGamma    = clamp(bestGamma, 0.15f, 0.8f);
    result.error       = bestError;
    result.sampleCount = static_cast<uint8_t>(valid.size());

    PF("[LuxCal] FIT luxMax=%.0f shiftLo=%d shiftHi=%d gamma=%.2f error=%.1f (n=%u)\n",
       result.luxMax, result.luxShiftLo, result.luxShiftHi, result.luxGamma,
       result.error, result.sampleCount);

    return true;
}

String LuxCalibration::buildJson() const {
    String json;
    json.reserve(128);
    json += F("{\"calibrationMode\":");
    json += Globals::luxCalibrationMode ? F("true") : F("false");
    json += F(",\"sampleCount\":");
    json += sampleCount();
    json += F(",\"lastLux\":");
    json += String(SensorController::ambientLux(), 1);
    json += F(",\"lastBrightness\":");
    json += String(Globals::lastUnclampedBrightness, 1);
    json += F(",\"hasLuxSensor\":");
    json += Globals::luxSensorPresent ? F("true") : F("false");
    json += '}';
    return json;
}
