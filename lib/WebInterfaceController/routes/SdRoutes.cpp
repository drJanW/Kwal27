/**
 * @file SdRoutes.cpp
 * @brief SD card API endpoint routes
 * @version 260217E
 * @date 2026-02-17
 */
#include "SdRoutes.h"
#include "../WebUtils.h"
#include "Globals.h"
#include "SDController.h"
#include "SD/SDBoot.h"
#include "Alert/AlertState.h"
#include <SD.h>

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
    if (AlertState::isSdBusy()) {
        sendError(request, 409, F("SD busy"));
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

    // Determine content type before SD access
    const char *contentType = "application/octet-stream";
    if (path.endsWith(".csv")) contentType = "text/csv";
    else if (path.endsWith(".txt")) contentType = "text/plain";
    else if (path.endsWith(".json")) contentType = "application/json";
    else if (path.endsWith(".html")) contentType = "text/html";
    else if (path.endsWith(".js")) contentType = "application/javascript";
    else if (path.endsWith(".css")) contentType = "text/css";

    // Read entire file into response stream under SD lock.
    // This keeps SD access short and prevents async file handle issues.
    SDController::lockSD();

    if (!SD.exists(path)) {
        SDController::unlockSD();
        sendError(request, 404, F("File not found"));
        return;
    }
    
    File file = SD.open(path, FILE_READ);
    if (!file || file.isDirectory()) {
        if (file) file.close();
        SDController::unlockSD();
        sendError(request, 400, F("Cannot read file"));
        return;
    }

    size_t fileSize = file.size();
    if (fileSize > 65536) {
        file.close();
        SDController::unlockSD();
        sendError(request, 413, F("File too large for download"));
        return;
    }

    // AsyncResponseStream owns its buffer — no manual free needed
    AsyncResponseStream *stream = request->beginResponseStream(contentType, fileSize);
    uint8_t chunk[512];
    size_t remaining = fileSize;
    while (remaining > 0) {
        size_t toRead = (remaining < sizeof(chunk)) ? remaining : sizeof(chunk);
        size_t got = file.read(chunk, toRead);
        if (got == 0) break;
        stream->write(chunk, got);
        remaining -= got;
    }
    file.close();
    SDController::unlockSD();

    request->send(stream);
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
        // Determine target path: use "path" query param if provided, else root
        String dir = "/";
        if (request->hasParam("path")) {
            dir = request->getParam("path")->value();
            if (!dir.startsWith("/")) dir = "/" + dir;
            if (!dir.endsWith("/")) dir += "/";
        }
        state->target = dir + filename;

        // Security: block directory traversal
        if (state->target.indexOf("..") >= 0) {
            state->failed = true;
            state->error = F("Invalid path");
            return;
        }

        SDController::lockSD();
        state->sdBusyClaimed = true;

        // Auto-create directory if needed
        if (dir.length() > 1 && !SD.exists(dir)) {
            SD.mkdir(dir);
        }

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

void routeListDir(AsyncWebServerRequest *request)
{
    if (!AlertState::isSdOk()) {
        sendError(request, 503, F("SD not ready"));
        return;
    }
    if (AlertState::isSdBusy()) {
        sendError(request, 409, F("SD busy"));
        return;
    }
    if (!request->hasParam("path")) {
        sendError(request, 400, F("Missing path parameter"));
        return;
    }

    String path = request->getParam("path")->value();
    if (!path.startsWith("/")) path = "/" + path;
    if (path.indexOf("..") >= 0) {
        sendError(request, 400, F("Invalid path"));
        return;
    }

    String payload = F("{\"files\":[");

    if (SD.exists(path)) {
        SDController::lockSD();
        File dir = SD.open(path);
        if (dir && dir.isDirectory()) {
            bool first = true;
            File entry = dir.openNextFile();
            while (entry) {
                if (!entry.isDirectory()) {
                    if (!first) payload += ',';
                    first = false;
                    payload += F("{\"name\":\"");
                    appendJsonEscaped(payload, entry.name());
                    payload += F("\",\"size\":");
                    payload += String(entry.size());
                    payload += '}';
                }
                entry.close();
                entry = dir.openNextFile();
            }
            dir.close();
        } else {
            if (dir) dir.close();
        }
        SDController::unlockSD();
    }

    payload += F("]}");
    sendJson(request, payload);
}

void routeDelete(AsyncWebServerRequest *request)
{
    if (!AlertState::isSdOk()) {
        sendError(request, 503, F("SD not ready"));
        return;
    }
    if (AlertState::isSdBusy()) {
        sendError(request, 409, F("SD busy"));
        return;
    }
    if (!request->hasParam("path")) {
        sendError(request, 400, F("Missing path parameter"));
        return;
    }

    String path = request->getParam("path")->value();
    if (!path.startsWith("/")) path = "/" + path;
    if (path.indexOf("..") >= 0) {
        sendError(request, 400, F("Invalid path"));
        return;
    }

    if (!SD.exists(path)) {
        sendError(request, 404, F("File not found"));
        return;
    }

    SDController::lockSD();
    bool ok = SD.remove(path);
    SDController::unlockSD();

    if (ok) {
        sendJson(request, F("{\"status\":\"ok\"}"));
    } else {
        sendError(request, 500, F("Delete failed"));
    }
}

void attachRoutes(AsyncWebServer &server)
{
    server.on("/api/sd/status", HTTP_GET, routeStatus);
    server.on("/api/sd/list", HTTP_GET, routeListDir);
    server.on("/api/sd/file", HTTP_GET, routeFileDownload);
    server.on("/api/sd/delete", HTTP_POST, routeDelete);
    server.on("/api/sd/upload", HTTP_POST, routeUploadRequest, routeUploadData);
    server.on("/api/sd/rebuild", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (!AlertState::isSdOk()) {
            sendError(request, 503, F("SD not ready"));
            return;
        }
        if (AlertState::isSdBusy()) {
            sendError(request, 409, F("SD busy"));
            return;
        }
        SDBoot::requestRebuild();
        sendJson(request, F("{\"status\":\"accepted\"}"));
    });
    server.on("/api/sd/syncdir", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (!AlertState::isSdOk()) {
            sendError(request, 503, F("SD not ready"));
            return;
        }
        if (!request->hasParam("dir")) {
            sendError(request, 400, F("Missing dir parameter"));
            return;
        }
        int dir = request->getParam("dir")->value().toInt();
        if (dir < 1 || dir > 200) {
            sendError(request, 400, F("Invalid dir (1-200)"));
            return;
        }
        SDBoot::requestSyncDir(static_cast<uint8_t>(dir));
        sendJson(request, F("{\"status\":\"accepted\"}"));
    });
}

} // namespace SdRoutes