/**
 * @file LuxCalibration.cpp
 * @brief Lux calibration data file and grid search fit implementation
 * @version 260310G
 * @date 2026-03-10
 */
#define LOCAL_LOG_LEVEL LOG_LEVEL_INFO
#include <Arduino.h>

#include "LuxCalibration.h"
#include "Globals.h"
#include "CsvUtils.h"
#include "SDController.h"
#include "SdPathUtils.h"
#include "SensorController.h"
#include "Alert/AlertState.h"
#include <SD.h>
#include <math.h>

namespace {
constexpr const char* luxCalPath  = "/luxcal.csv";
constexpr const char* csvHeader   = "lux;brightness;patternId;colorsId";
constexpr float       seedLuxMax  = 300.0f;  // fixed low start — breaks luxMax snowball
} // namespace

LuxCalibration& LuxCalibration::instance() {
    static LuxCalibration inst;
    return inst;
}

void LuxCalibration::addSample(const LuxCalSample& sample) {
    if (samples_.size() >= Globals::maxLuxDataPoints) {
        samples_.erase(samples_.begin());
    }
    samples_.push_back(sample);
    ++realCount_;
    PF("[LuxCal] Data point added: lux=%.1f bri=%.1f pat=%u col=%u (n=%u real=%u)\n",
       sample.lux, sample.brightness, sample.patternId, sample.colorsId,
       static_cast<unsigned>(samples_.size()), realCount_);
}

void LuxCalibration::clearSamples() {
    samples_.clear();
    realCount_ = 0;
    PL("[LuxCal] Data points cleared");
}

bool LuxCalibration::generateSeeds() {
    samples_.clear();
    realCount_ = 0;
    uint8_t n = Globals::seededLuxDataPoints;
    samples_.reserve(n);
    for (uint8_t i = 0; i < n; ++i) {
        float normLux = static_cast<float>(i + 1) / n;  // 1/n .. 1.0
        normLux *= normLux;                              // quadratic: more seeds at low lux
        float seedLux = normLux * seedLuxMax;
        float luxT = powf(normLux, Globals::luxGamma);
        float luxShift = Globals::luxShiftLo + (Globals::luxShiftHi - Globals::luxShiftLo) * luxT;
        LuxCalSample s;
        s.lux        = seedLux;
        s.brightness = Globals::brightnessHi * (1.0f + luxShift / 100.0f);
        s.patternId  = 1;
        s.colorsId   = 1;
        samples_.push_back(s);
    }
    PF("[LuxCal] Generated %u seeds from current params\n", n);
    return saveToSd();
}

bool LuxCalibration::loadFromSd() {
    if (!AlertState::isSdOk()) return false;
    if (!SDController::fileExists(luxCalPath)) return false;

    File file = SDController::openFileRead(luxCalPath);
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
    // realCount_ = total loaded minus seed count (seeds were first N entries)
    uint8_t total = static_cast<uint8_t>(samples_.size());
    if (total > Globals::seededLuxDataPoints) {
        realCount_ = total - Globals::seededLuxDataPoints;
    } else {
        realCount_ = 0;
    }
    PF("[LuxCal] Loaded %u data points from SD (real=%u)\n", total, realCount_);
    return true;
}

bool LuxCalibration::saveToSd() const {
    if (!AlertState::isSdOk()) return false;

    SDController::deleteFile(luxCalPath);
    File file = SDController::openFileWrite(luxCalPath);
    if (!file) return false;

    file.println(csvHeader);
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
    SDController::deleteFile(luxCalPath);
    PL("[LuxCal] CSV deleted");
    return true;
}

bool LuxCalibration::fitParams(LuxFitResult& result) const {
    constexpr int   fitShiftLimit = 100;  // max absolute shift searched by grid fit
    constexpr float fitGammaMax   = 2.0f; // max gamma searched by grid fit

    if (samples_.empty()) {
        PL("[LuxCal] FIT: no data points");
        return false;
    }

    // Determine luxMax search candidates from observed data
    float maxLux = 0.0f;
    for (const auto& s : samples_) {
        if (s.lux > maxLux) maxLux = s.lux;
    }
    if (maxLux < 10.0f) maxLux = 10.0f;
    const float luxMaxCandidates[] = {
        maxLux, maxLux * 1.1f, maxLux * 1.2f, maxLux * 1.3f, maxLux * 1.5f
    };
    constexpr uint8_t numLuxMax = 5;

    // MSE calculator — inverse-lux weighting
    auto calcMse = [&](float fitMax, int sLo, int sHi, float gamma) -> float {
        float totalError = 0.0f;
        float totalWeight = 0.0f;

        for (const auto& s : samples_) {
            float normLux = clamp(s.lux, Globals::luxMin, fitMax) / fitMax;
            float luxT = powf(normLux, gamma);
            float luxShift = sLo + (sHi - sLo) * luxT;
            float predicted = Globals::brightnessHi * (1.0f + luxShift / 100.0f);
            float diff = s.brightness - predicted;
            float w = 1.0f / (1.0f + s.lux);
            totalError += diff * diff * w;
            totalWeight += w;
        }

        return (totalWeight > 0.0f) ? totalError / totalWeight : 1e12f;
    };

    // Grid search pass 1: coarse (full parameter space)
    float bestError = 1e12f;
    float bestLuxMax = maxLux;
    int bestShiftLo = 0;
    int bestShiftHi = 0;
    float bestGamma = 0.4f;

    for (uint8_t m = 0; m < numLuxMax; ++m) {
        float fitMax = luxMaxCandidates[m];
        for (int sLo = -fitShiftLimit; sLo <= 0; sLo += 5) {
            for (int sHi = 0; sHi <= fitShiftLimit; sHi += 5) {
                for (int gIdx = 2; gIdx <= static_cast<int>(fitGammaMax * 10.0f); gIdx++) {
                    float gamma = gIdx * 0.1f;
                    float mse = calcMse(fitMax, sLo, sHi, gamma);
                    if (mse < bestError) {
                        bestError = mse;
                        bestLuxMax = fitMax;
                        bestShiftLo = sLo;
                        bestShiftHi = sHi;
                        bestGamma = gamma;
                    }
                }
            }
        }
    }

    // Grid search pass 2: fine (±4 shift, ±0.1 gamma, ±5% luxMax)
    float luxMaxFine[] = { bestLuxMax * 0.95f, bestLuxMax, bestLuxMax * 1.05f };
    for (uint8_t m = 0; m < 3; ++m) {
        float fitMax = luxMaxFine[m];
        if (fitMax < 10.0f) continue;
        for (int sLo = bestShiftLo - 4; sLo <= bestShiftLo + 4; sLo++) {
            if (sLo < -fitShiftLimit || sLo > 0) continue;
            for (int sHi = bestShiftHi - 4; sHi <= bestShiftHi + 4; sHi++) {
                if (sHi < 0 || sHi > fitShiftLimit) continue;
                for (int gIdx = static_cast<int>(bestGamma * 20.0f) - 2;
                         gIdx <= static_cast<int>(bestGamma * 20.0f) + 2; gIdx++) {
                    float gamma = gIdx * 0.05f;
                    if (gamma < 0.1f || gamma > fitGammaMax) continue;
                    float mse = calcMse(fitMax, sLo, sHi, gamma);
                    if (mse < bestError) {
                        bestError = mse;
                        bestLuxMax = fitMax;
                        bestShiftLo = sLo;
                        bestShiftHi = sHi;
                        bestGamma = gamma;
                    }
                }
            }
        }
    }

    // Round luxMax to integer for cleaner output
    result.luxMax      = roundf(bestLuxMax);
    result.luxShiftLo  = bestShiftLo;
    result.luxShiftHi  = bestShiftHi;
    result.luxGamma    = clamp(bestGamma, 0.1f, fitGammaMax);
    result.error       = bestError;
    result.sampleCount = static_cast<uint8_t>(samples_.size());

    PF("[LuxCal] FIT luxMax=%.0f shiftLo=%d shiftHi=%d gamma=%.2f error=%.1f (n=%u)\n",
       result.luxMax, result.luxShiftLo, result.luxShiftHi, result.luxGamma,
       result.error, result.sampleCount);

    return true;
}

bool LuxCalibration::saveFittedParams(const LuxFitResult& result) const {
    if (!AlertState::isSdOk()) return false;
    const String csvPath = SdPathUtils::chooseCsvPath("globals.csv");
    if (csvPath.isEmpty()) return false;

    File file = SD.open(csvPath.c_str(), FILE_APPEND);
    if (!file) return false;

    file.printf("\n# ── Lux calibration fit ──\n");
    file.printf("luxMax;f;%.1f;fitted lux ceiling\n", result.luxMax);
    file.printf("luxShiftLo;i;%d;fitted dark shift\n", result.luxShiftLo);
    file.printf("luxShiftHi;i;%d;fitted bright shift\n", result.luxShiftHi);
    file.printf("luxGamma;f;%.2f;fitted gamma\n", static_cast<double>(result.luxGamma));
    file.close();

    PF("[LuxCal] Fitted params saved to %s\n", csvPath.c_str());
    return true;
}

String LuxCalibration::buildJson() const {
    String json;
    json.reserve(128);
    json += F("{\"calibrationMode\":");
    json += Globals::luxCalibrationMode ? F("true") : F("false");
    json += F(",\"sampleCount\":");
    json += sampleCount();    json += F(",\"realCount\":");
    json += realCount_;    json += F(",\"lastLux\":");
    json += String(SensorController::ambientLux(), 1);
    json += F(",\"lastBrightness\":");
    json += String(Globals::lastUnclampedBrightness, 1);
    json += F(",\"hasLuxSensor\":");
    json += Globals::luxSensorPresent ? F("true") : F("false");
    json += F(",\"luxMax\":");
    json += String(Globals::luxMax, 0);
    json += F(",\"luxShiftLo\":");
    json += Globals::luxShiftLo;
    json += F(",\"luxShiftHi\":");
    json += Globals::luxShiftHi;
    json += F(",\"luxGamma\":");
    json += String(Globals::luxGamma, 2);
    json += '}';
    return json;
}
