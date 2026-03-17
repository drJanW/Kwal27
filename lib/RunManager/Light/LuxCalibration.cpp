/**
 * @file LuxCalibration.cpp
 * @brief Lux calibration data file and Gauss-Newton fit implementation
 * @version 260317E
 * @date 2026-03-13
 */
#define LOCAL_LOG_LEVEL LOG_LEVEL_INFO
#include <Arduino.h>

#include "LuxCalibration.h"
#include "Globals.h"
#include "CsvUtils.h"
#include "SDController.h"
#include "SensorController.h"
#include "Alert/AlertState.h"
#include <SD.h>
#include <math.h>

namespace {
constexpr const char* luxCalPath  = "/luxcal.csv";
constexpr const char* csvHeader   = "lux;brightness;nf";
} // namespace

LuxCalibration& LuxCalibration::instance() {
    static LuxCalibration inst;
    return inst;
}

bool LuxCalibration::isFull() const {
    return samples_.size() >= Globals::maxLuxDataPoints;
}

void LuxCalibration::addSample(const LuxCalSample& sample) {
    samples_.push_back(sample);
    ++realCount_;
    // luxMax is a physical boundary — grow on new high-water mark
    if (sample.lux > Globals::luxMax) {
        Globals::luxMax = ceilf(sample.lux);
        PF("[LuxCal] luxMax grown to %.0f\n", Globals::luxMax);
    }
    PF("[LuxCal] Data point added: lux=%.1f bri=%.1f nf=%.3f (n=%u real=%u)\n",
       sample.lux, sample.brightness, sample.nf,
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
    Globals::luxMax = 300.0f;  // reset range to default
    uint8_t n = Globals::seededLuxDataPoints;
    samples_.reserve(n);
    for (uint8_t i = 0; i < n; ++i) {
        float normLux = static_cast<float>(i + 1) / n;  // 1/n .. 1.0
        normLux *= normLux;                              // quadratic: more seeds at low lux
        float seedLux = normLux * Globals::luxMax;
        LuxCalSample s;
        s.lux        = seedLux;
        s.brightness = Globals::brMax * (1.0f - expf(-Globals::luxRate * seedLux));
        s.nf         = 1.0f;
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
        if (columns.size() < 3) continue;

        LuxCalSample s;
        s.lux        = columns[0].toFloat();
        s.brightness = columns[1].toFloat();
        s.nf         = columns[2].toFloat();
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
        file.println(s.nf, 3);
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
    if (samples_.size() < 2) {
        PL("[LuxCal] FIT: need at least 2 data points");
        return false;
    }

    // Gauss-Newton: fit on brightness*nf vs lux
    // Model: y = B * (1 - exp(-r * lux))
    // dY/dB = 1 - exp(-r * lux)            = e
    // dY/dr = B * lux * exp(-r * lux)       = B * lux * (1 - e)
    // Weight: w = 1 / (1 + lux)  — favours low-lux accuracy

    float B = Globals::brMax;      // start from current params
    float r = Globals::luxRate;
    constexpr uint8_t maxIter = 12;
    constexpr float   minStep = 1e-6f;

    for (uint8_t iter = 0; iter < maxIter; ++iter) {
        // accumulate J^T W J (2x2) and J^T W residual (2x1)
        float jj00 = 0, jj01 = 0, jj11 = 0;   // symmetric: jj10 = jj01
        float jr0  = 0, jr1  = 0;

        for (const auto& s : samples_) {
            float lux = MathUtils::maxVal(s.lux, 0.0f);
            float observed = s.brightness * s.nf;            // normalized observed value
            float e   = 1.0f - expf(-r * lux);              // shared sub-expression
            float predicted = B * e;
            float residual  = observed - predicted;
            float w = 1.0f / (1.0f + lux);

            float j0 = e;                    // dY/dB
            float j1 = B * lux * (1.0f - e); // dY/dr

            float wj0 = w * j0;
            float wj1 = w * j1;
            jj00 += wj0 * j0;
            jj01 += wj0 * j1;
            jj11 += wj1 * j1;
            jr0  += wj0 * residual;
            jr1  += wj1 * residual;
        }

        // solve 2x2: [jj00 jj01; jj01 jj11] * [dB; dr] = [jr0; jr1]
        float det = jj00 * jj11 - jj01 * jj01;
        if (fabsf(det) < 1e-20f) break;   // singular — stop

        float dB = ( jj11 * jr0 - jj01 * jr1) / det;
        float dr = (-jj01 * jr0 + jj00 * jr1) / det;

        B += dB;
        r += dr;

        // clamp to sane range
        if (B < 10.0f)  B = 10.0f;
        if (B > 500.0f) B = 500.0f;
        if (r < 0.001f) r = 0.001f;
        if (r > 0.5f)   r = 0.5f;

        if (fabsf(dB) < minStep && fabsf(dr) < minStep) break;  // converged
    }

    // compute R² = 1 - SSres/SStot  (on brightness*nf)
    float ssRes = 0, meanY = 0;
    for (const auto& s : samples_) meanY += s.brightness * s.nf;
    meanY /= samples_.size();
    float ssTot = 0;
    for (const auto& s : samples_) {
        float lux = MathUtils::maxVal(s.lux, 0.0f);
        float observed = s.brightness * s.nf;
        float diff = observed - B * (1.0f - expf(-r * lux));
        ssRes += diff * diff;
        float dm = observed - meanY;
        ssTot += dm * dm;
    }

    result.brMax       = B;
    result.luxRate     = r;
    result.r2          = (ssTot > 0) ? 1.0f - ssRes / ssTot : 0.0f;
    result.sampleCount = static_cast<uint8_t>(samples_.size());

    PF("[LuxCal] FIT brMax=%.1f rate=%.4f R²=%.4f (n=%u)\n",
       result.brMax, static_cast<double>(result.luxRate),
       static_cast<double>(result.r2), result.sampleCount);

    return true;
}

// Persist fitted calibration params to /config.txt (per-device file).
// Uses read-modify-write: existing brMax=/luxRate=/luxMax= lines are
// updated in-place; missing keys are appended.
// config.txt is never overwritten by NAS CSV fetch, so calibration
// survives reboots and upload_csv.ps1 runs.
// Only Cal Lux Reset (🔄 in WebGUI) regenerates seeds from Globals.h defaults.
bool LuxCalibration::saveFittedParams(const LuxFitResult& result) const {
    if (!AlertState::isSdOk()) return false;
    const char* path = "/config.txt";

    // Read existing config.txt, update or append brMax/luxRate/luxMax
    File readFile = SD.open(path, FILE_READ);
    String content;
    bool hasBrMax = false, hasLuxRate = false, hasLuxMax = false;
    if (readFile) {
        while (readFile.available()) {
            String line = readFile.readStringUntil('\n');
            line.trim();
            if (line.startsWith("brMax=")) {
                line = "brMax=" + String(result.brMax, 1);
                hasBrMax = true;
            } else if (line.startsWith("luxRate=")) {
                line = "luxRate=" + String(static_cast<double>(result.luxRate), 4);
                hasLuxRate = true;
            } else if (line.startsWith("luxMax=")) {
                line = "luxMax=" + String(Globals::luxMax, 1);
                hasLuxMax = true;
            }
            content += line + "\n";
        }
        readFile.close();
    }
    if (!hasBrMax)  content += "brMax=" + String(result.brMax, 1) + "\n";
    if (!hasLuxRate) content += "luxRate=" + String(static_cast<double>(result.luxRate), 4) + "\n";
    if (!hasLuxMax) content += "luxMax=" + String(Globals::luxMax, 1) + "\n";

    File writeFile = SD.open(path, FILE_WRITE);
    if (!writeFile) return false;
    writeFile.print(content);
    writeFile.close();

    PF("[LuxCal] Fitted params saved to %s\n", path);
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
    json += String(Globals::lastFastledBrightness, 1);
    json += F(",\"hasLuxSensor\":");
    json += Globals::luxSensorPresent ? F("true") : F("false");
    json += F(",\"luxMax\":");
    json += String(Globals::luxMax, 0);
    json += F(",\"brMax\":");
    json += String(Globals::brMax, 1);
    json += F(",\"luxRate\":");
    json += String(Globals::luxRate, 4);
    json += '}';
    return json;
}
