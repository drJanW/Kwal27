/**
 * @file SdRoutes.cpp
 * @brief SD card API endpoint routes
 * @version 260205A
 * @date 2026-02-05
 */
#include "SdRoutes.h"
#include "../WebUtils.h"
#include "Globals.h"
#include "SDController.h"
#include "Alert/AlertState.h"
#include <SD.h>
#include <memory>

using WebUtils::sendJson;
using WebUtils::sendError;
using WebUtils::appendJsonEscaped;

namespace SdRoutes {

void routeStatus(AsyncWebServerRequest *request)
{
    const bool ready = AlertState::isSdOk();
    const bool busy = AlertState::isSdBusy();
    const bool hasIndex = ready && SDController::fileExists("/index.html");

    String payload = F("{");
    payload += F("\"ready\":");
    payload += ready ? F("true") : F("false");
    payload += F(",\"busy\":");
    payload += busy ? F("true") : F("false");
    payload += F(",\"hasIndex\":");
    payload += hasIndex ? F("true") : F("false");
    payload += F("}");

    sendJson(request, payload);
}

void routeFileDownload(AsyncWebServerRequest *request)
{
    if (!AlertState::isSdOk()) {
        sendError(request, 503, F("SD not ready"));
        return;
    }
    
    if (!request->hasParam("path")) {
        sendError(request, 400, F("Missing path parameter"));
        return;
    }
    
    String path = request->getParam("path")->value();
    
    // Ensure path starts with /
    if (!path.startsWith("/")) {
        path = "/" + path;
    }
    
    // Security: block directory traversal
    if (path.indexOf("..") >= 0) {
        sendError(request, 400, F("Invalid path"));
        return;
    }
    
    if (!SD.exists(path)) {
        sendError(request, 404, F("File not found"));
        return;
    }
    
    File file = SD.open(path, FILE_READ);
    if (!file || file.isDirectory()) {
        if (file) file.close();
        sendError(request, 400, F("Cannot read file"));
        return;
    }
    
    // Determine content type
    String contentType = "application/octet-stream";
    if (path.endsWith(".csv")) contentType = "text/csv";
    else if (path.endsWith(".txt")) contentType = "text/plain";
    else if (path.endsWith(".json")) contentType = "application/json";
    else if (path.endsWith(".html")) contentType = "text/html";
    else if (path.endsWith(".js")) contentType = "application/javascript";
    else if (path.endsWith(".css")) contentType = "text/css";
    
    request->send(SD, path, contentType);
}

// Upload state for multipart routing
struct UploadState
{
    File file;
    String target;
    bool failed = false;
    String error;
    bool sdBusyClaimed = false;
};

void routeUploadRequest(AsyncWebServerRequest *request)
{
    std::unique_ptr<UploadState> state(static_cast<UploadState *>(request->_tempObject));
    request->_tempObject = nullptr;

    if (!state) {
        sendError(request, 500, F("Upload state missing"));
        return;
    }
    if (state->failed) {
        sendError(request, 400, state->error.length() ? state->error : F("Upload failed"));
        return;
    }

    String payload = F("{\"status\":\"ok\",\"path\":\"");
    appendJsonEscaped(payload, state->target.c_str());
    payload += F("\"}");
    sendJson(request, payload);
}

void routeUploadData(AsyncWebServerRequest *request, const String &filename,
                      size_t index, uint8_t *data, size_t len, bool final)
{
    UploadState *state = static_cast<UploadState *>(request->_tempObject);
    if (!state) {
        state = new UploadState();
        request->_tempObject = state;
    }

    if (state->failed) {
        if (final && state->file) {
            state->file.close();
            if (state->sdBusyClaimed) {
                SDController::unlockSD();
                state->sdBusyClaimed = false;
            }
        }
        return;
    }

    if (index == 0) {
        // Always upload to root directory
        state->target = "/" + filename;
        SDController::lockSD();
        state->sdBusyClaimed = true;
        state->file = SD.open(state->target.c_str(), FILE_WRITE);
        if (!state->file) {
            state->failed = true;
            state->error = F("Cannot open target file");
            SDController::unlockSD();
            state->sdBusyClaimed = false;
            return;
        }
    }

    if (len > 0 && state->file) {
        if (state->file.write(data, len) != len) {
            state->failed = true;
            state->error = F("Write failed");
            state->file.close();
            if (state->sdBusyClaimed) {
                SDController::unlockSD();
                state->sdBusyClaimed = false;
            }
        }
    }

    if (final) {
        if (state->file) {
            state->file.close();
        }
        if (state->sdBusyClaimed) {
            SDController::unlockSD();
            state->sdBusyClaimed = false;
        }
    }
}

void attachRoutes(AsyncWebServer &server)
{
    server.on("/api/sd/status", HTTP_GET, routeStatus);
    server.on("/api/sd/file", HTTP_GET, routeFileDownload);
    server.on("/api/sd/upload", HTTP_POST, routeUploadRequest, routeUploadData);
}

} // namespace SdRoutes