/**
 * @file LuxCalRoutes.cpp
 * @brief Lux calibration API endpoint routes implementation
 * @version 260303A
 * @date 2026-03-03
 */
#include <Arduino.h>
#include "LuxCalRoutes.h"
#include "../WebUtils.h"
#include "Globals.h"
#include "Light/LuxCalibration.h"
#include "Light/LightRun.h"
#include "Alert/AlertState.h"
#include "SensorController.h"
#include "SDController.h"
#include <SD.h>

using WebUtils::sendJson;
using WebUtils::sendError;

namespace LuxCalRoutes {

// POST /api/lux/calibrate?mode=on|off
// Sets calibration mode (memory only, hard stop 6)
static void routeCalibrate(AsyncWebServerRequest* request) {
    if (!request->hasParam("mode")) {
        sendError(request, 400, F("Missing ?mode=on|off"));
        return;
    }
    String mode = request->getParam("mode")->value();
    if (mode == "on") {
        Globals::luxCalibrationMode = true;
        PL("[LuxCal] Calibration mode ON");
    } else {
        Globals::luxCalibrationMode = false;
        PL("[LuxCal] Calibration mode OFF");
    }
    sendJson(request, LuxCalibration::instance().buildJson());
}

// POST /api/lux/sample
// Sets flag → cb_measureLux will capture the sample
static void routeSample(AsyncWebServerRequest* request) {
    if (!Globals::luxCalibrationMode) {
        sendError(request, 409, F("Calibration mode is off"));
        return;
    }
    Globals::luxCalSampleRequested = true;
    // Trigger a fresh lux measurement cycle (fade-out → read → fade-in)
    LightRun::requestLuxMeasurement();

    // Return current state (sample will be captured asynchronously)
    String json;
    json.reserve(96);
    json += F("{\"ok\":true,\"lux\":");
    json += String(SensorController::ambientLux(), 1);
    json += F(",\"brightness\":");
    json += String(Globals::lastUnclampedBrightness, 1);
    json += F(",\"n\":");
    json += LuxCalibration::instance().sampleCount();
    json += '}';
    sendJson(request, json);
}

// GET /api/lux/status
static void routeStatus(AsyncWebServerRequest* request) {
    sendJson(request, LuxCalibration::instance().buildJson());
}

// POST /api/lux/fit — trigger grid search fit, return results
static void routeFit(AsyncWebServerRequest* request) {
    LuxFitResult result;
    if (!LuxCalibration::instance().fitParams(result)) {
        sendError(request, 422, F("Too few samples (need 4+)"));
        return;
    }
    // Apply fitted params to memory
    Globals::luxMax     = result.luxMax;
    Globals::luxShiftLo = result.luxShiftLo;
    Globals::luxShiftHi = result.luxShiftHi;
    Globals::luxGamma   = result.luxGamma;

    String json;
    json.reserve(160);
    json += F("{\"ok\":true,\"luxMax\":");
    json += String(result.luxMax, 0);
    json += F(",\"luxShiftLo\":");
    json += result.luxShiftLo;
    json += F(",\"luxShiftHi\":");
    json += result.luxShiftHi;
    json += F(",\"luxGamma\":");
    json += String(result.luxGamma, 2);
    json += F(",\"error\":");
    json += String(result.error, 1);
    json += F(",\"sampleCount\":");
    json += result.sampleCount;
    json += '}';
    sendJson(request, json);
}

// POST /api/lux/clear — reset samples + delete CSV
static void routeClear(AsyncWebServerRequest* request) {
    LuxCalibration::instance().clearSamples();
    if (AlertState::isSdOk() && !AlertState::isSdBusy()) {
        LuxCalibration::instance().deleteCsv();
    }
    sendJson(request, F("{\"ok\":true,\"sampleCount\":0}"));
}

// GET /api/lux/csv — serve CSV as download
static void routeCsvDownload(AsyncWebServerRequest* request) {
    if (!AlertState::isSdOk()) {
        sendError(request, 503, F("SD not available"));
        return;
    }
    if (!SDController::fileExists("/luxcal.csv")) {
        sendError(request, 404, F("No luxcal.csv"));
        return;
    }
    request->send(SD, "/luxcal.csv", "text/csv");
}

// POST /api/lux/reload — re-read CSV into RAM
static void routeReload(AsyncWebServerRequest* request) {
    LuxCalibration::instance().loadFromSd();
    sendJson(request, LuxCalibration::instance().buildJson());
}

void attachRoutes(AsyncWebServer& server) {
    server.on("/api/lux/calibrate", HTTP_POST, routeCalibrate);
    server.on("/api/lux/sample",    HTTP_POST, routeSample);
    server.on("/api/lux/status",    HTTP_GET,  routeStatus);
    server.on("/api/lux/fit",       HTTP_POST, routeFit);
    server.on("/api/lux/clear",     HTTP_POST, routeClear);
    server.on("/api/lux/csv",       HTTP_GET,  routeCsvDownload);
    server.on("/api/lux/reload",    HTTP_POST, routeReload);
}

} // namespace LuxCalRoutes
