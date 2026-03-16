/**
 * @file LuxCalRoutes.cpp
 * @brief Lux calibration API endpoint routes implementation
 * @version 260313C
 * @date 2026-03-13
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
#include "PRTClock.h"
#include <SD.h>

#include "RunManager.h"

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
        RunManager::requestStopAudio();
        RunManager::touchCalActivity();
        PL("[LuxCal] Calibration mode ON");
        // Load existing data or generate seeds from current params
        auto& cal = LuxCalibration::instance();
        if (!cal.loadFromSd() && AlertState::isSdOk() && !AlertState::isSdBusy()) {
            cal.generateSeeds();
        }
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
    if (LuxCalibration::instance().isFull()) {
        sendError(request, 409, F("Buffer full — fit or clear first"));
        return;
    }
    Globals::luxCalSampleRequested = true;
    RunManager::touchCalActivity();
    // Trigger a fresh lux measurement cycle (fade-out → read → fade-in)
    LightRun::requestLuxMeasurement();

    // Return current state (sample will be captured asynchronously)
    String json;
    json.reserve(112);
    json += F("{\"ok\":true,\"lux\":");
    json += String(SensorController::ambientLux(), 1);
    json += F(",\"brightness\":");
    json += String(Globals::lastFastledBrightness, 1);
    json += F(",\"n\":");
    json += LuxCalibration::instance().sampleCount();
    json += F(",\"realCount\":");
    json += LuxCalibration::instance().realCount();
    json += '}';
    sendJson(request, json);
}

// GET /api/lux/status
static void routeStatus(AsyncWebServerRequest* request) {
    sendJson(request, LuxCalibration::instance().buildJson());
}

// POST /api/lux/fit — calculate fit, report to UI, do NOT apply to Globals
static void routeFit(AsyncWebServerRequest* request) {
    LuxFitResult result;
    auto& cal = LuxCalibration::instance();
    RunManager::touchCalActivity();
    if (!cal.fitParams(result)) {
        sendError(request, 422, F("No data points"));
        return;
    }
    // Store for later accept — Globals remain untouched
    cal.setPendingFit(result);

    String json;
    json.reserve(256);
    json += F("{\"ok\":true,\"brMax\":");
    json += String(result.brMax, 1);
    json += F(",\"luxRate\":");
    json += String(result.luxRate, 4);
    json += F(",\"r2\":");
    json += String(result.r2, 4);
    json += F(",\"luxMax\":");
    json += String(Globals::luxMax, 0);
    json += F(",\"sampleCount\":");
    json += result.sampleCount;
    json += F(",\"realCount\":");
    json += LuxCalibration::instance().realCount();
    json += F(",\"oldBrMax\":");
    json += String(Globals::brMax, 1);
    json += F(",\"oldLuxRate\":");
    json += String(Globals::luxRate, 4);
    json += '}';
    sendJson(request, json);
}

// POST /api/lux/accept — apply pending fit to Globals + persist to globals.csv
static void routeAcceptFit(AsyncWebServerRequest* request) {
    auto& cal = LuxCalibration::instance();
    RunManager::touchCalActivity();
    if (!cal.hasPendingFit()) {
        sendError(request, 409, F("No pending fit"));
        return;
    }
    const auto& fit = cal.getPendingFit();

    // Apply fit to Globals
    Globals::brMax    = fit.brMax;
    Globals::luxRate  = fit.luxRate;

    bool saved = false;
    if (AlertState::isSdOk() && !AlertState::isSdBusy()) {
        saved = cal.saveFittedParams(fit);
        cal.generateSeeds();
    }
    cal.clearPendingFit();

    String json;
    json.reserve(64);
    json += F("{\"ok\":");
    json += saved ? F("true") : F("false");
    json += F(",\"sampleCount\":");
    json += LuxCalibration::instance().sampleCount();
    json += F(",\"realCount\":");
    json += LuxCalibration::instance().realCount();
    json += '}';
    sendJson(request, json);
}

// POST /api/lux/clear — reset to seeds from current params
static void routeClear(AsyncWebServerRequest* request) {
    if (AlertState::isSdOk() && !AlertState::isSdBusy()) {
        LuxCalibration::instance().generateSeeds();
    } else {
        LuxCalibration::instance().clearSamples();
    }
    String json;
    json.reserve(48);
    json += F("{\"ok\":true,\"sampleCount\":");
    json += LuxCalibration::instance().sampleCount();
    json += '}';
    sendJson(request, json);
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
    char filename[32];
    snprintf(filename, sizeof(filename), "luxdata_%02u%02u%02u.csv",
             prtClock.getDay(), prtClock.getHour(), prtClock.getMinute());
    AsyncWebServerResponse* response = request->beginResponse(SD, "/luxcal.csv", "text/csv");
    char header[64];
    snprintf(header, sizeof(header), "attachment; filename=\"%s\"", filename);
    response->addHeader("Content-Disposition", header);
    request->send(response);
}

// POST /api/lux/reload — re-read CSV into RAM
static void routeReload(AsyncWebServerRequest* request) {
    LuxCalibration::instance().loadFromSd();
    sendJson(request, LuxCalibration::instance().buildJson());
}

// GET /api/lux/points — all data points for chart rendering
static void routePoints(AsyncWebServerRequest* request) {
    auto& cal = LuxCalibration::instance();
    const auto& pts = cal.samples();
    uint8_t seedCount = Globals::seededLuxDataPoints;

    String json;
    json.reserve(64 + pts.size() * 40);
    json += F("{\"luxMax\":");
    json += String(Globals::luxMax, 0);
    json += F(",\"brMax\":");
    json += String(Globals::brMax, 1);
    json += F(",\"luxRate\":");
    json += String(Globals::luxRate, 4);
    json += F(",\"seedCount\":");
    json += seedCount;
    json += F(",\"full\":");
    json += cal.isFull() ? F("true") : F("false");
    json += F(",\"points\":[");
    for (size_t i = 0; i < pts.size(); ++i) {
        if (i > 0) json += ',';
        json += F("{\"lux\":");
        json += String(pts[i].lux, 1);
        json += F(",\"bri\":");
        json += String(pts[i].brightness * pts[i].nf, 1);
        json += F(",\"seed\":");
        json += (i < seedCount) ? F("true") : F("false");
        json += '}';
    }
    json += F("]}");
    sendJson(request, json);
}

void attachRoutes(AsyncWebServer& server) {
    server.on("/api/lux/calibrate", HTTP_POST, routeCalibrate);
    server.on("/api/lux/sample",    HTTP_POST, routeSample);
    server.on("/api/lux/status",    HTTP_GET,  routeStatus);
    server.on("/api/lux/fit",       HTTP_POST, routeFit);
    server.on("/api/lux/accept",    HTTP_POST, routeAcceptFit);
    server.on("/api/lux/clear",     HTTP_POST, routeClear);
    server.on("/api/lux/csv",       HTTP_GET,  routeCsvDownload);
    server.on("/api/lux/points",    HTTP_GET,  routePoints);
    server.on("/api/lux/reload",    HTTP_POST, routeReload);
}

} // namespace LuxCalRoutes
