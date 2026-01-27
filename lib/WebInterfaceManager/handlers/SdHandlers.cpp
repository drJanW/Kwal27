/**
 * @file SdHandlers.cpp
 * @brief SD card API endpoint handlers
 * @version 251231E
 * @date 2025-12-31
 *
 * Implements HTTP handlers for the /api/sd/* endpoints.
 * Provides routes to check SD card status (ready, busy, hasIndex),
 * download files from SD, and handle file uploads to the SD card.
 * Integrates with SDManager for safe file operations.
 */

#include "SdHandlers.h"
#include "../WebUtils.h"
#include "Globals.h"
#include "SDManager.h"
#include <SD.h>
#include <memory>

using WebUtils::sendJson;
using WebUtils::sendError;
using WebUtils::appendJsonEscaped;

namespace SdHandlers {

void handleStatus(AsyncWebServerRequest *request)
{
    const bool ready = SDManager::isReady();
    const bool busy = SDManager::isSDbusy();
    const bool hasIndex = ready && SDManager::instance().fileExists("/index.html");

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

void handleFileDownload(AsyncWebServerRequest *request)
{
    if (!SDManager::isReady()) {
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

// Upload context for multipart handling
struct UploadContext
{
    File file;
    String target;
    bool failed = false;
    String error;
    bool sdBusyClaimed = false;
};

void handleUploadRequest(AsyncWebServerRequest *request)
{
    std::unique_ptr<UploadContext> ctx(static_cast<UploadContext *>(request->_tempObject));
    request->_tempObject = nullptr;

    if (!ctx) {
        sendError(request, 500, F("Upload context missing"));
        return;
    }
    if (ctx->failed) {
        sendError(request, 400, ctx->error.length() ? ctx->error : F("Upload failed"));
        return;
    }

    String payload = F("{\"status\":\"ok\",\"path\":\"");
    appendJsonEscaped(payload, ctx->target.c_str());
    payload += F("\"}");
    sendJson(request, payload);
}

void handleUploadData(AsyncWebServerRequest *request, const String &filename,
                      size_t index, uint8_t *data, size_t len, bool final)
{
    UploadContext *ctx = static_cast<UploadContext *>(request->_tempObject);
    if (!ctx) {
        ctx = new UploadContext();
        request->_tempObject = ctx;
    }

    if (ctx->failed) {
        if (final && ctx->file) {
            ctx->file.close();
            if (ctx->sdBusyClaimed) {
                SDManager::setSDbusy(false);
                ctx->sdBusyClaimed = false;
            }
        }
        return;
    }

    if (index == 0) {
        // Always upload to root directory
        ctx->target = "/" + filename;
        if (SDManager::isSDbusy()) {
            ctx->failed = true;
            ctx->error = F("SD busy");
            return;
        }
        SDManager::setSDbusy(true);
        ctx->sdBusyClaimed = true;
        ctx->file = SD.open(ctx->target.c_str(), FILE_WRITE);
        if (!ctx->file) {
            ctx->failed = true;
            ctx->error = F("Cannot open target file");
            SDManager::setSDbusy(false);
            ctx->sdBusyClaimed = false;
            return;
        }
    }

    if (len > 0 && ctx->file) {
        if (ctx->file.write(data, len) != len) {
            ctx->failed = true;
            ctx->error = F("Write failed");
            ctx->file.close();
            if (ctx->sdBusyClaimed) {
                SDManager::setSDbusy(false);
                ctx->sdBusyClaimed = false;
            }
        }
    }

    if (final) {
        if (ctx->file) {
            ctx->file.close();
        }
        if (ctx->sdBusyClaimed) {
            SDManager::setSDbusy(false);
            ctx->sdBusyClaimed = false;
        }
    }
}

void attachRoutes(AsyncWebServer &server)
{
    server.on("/api/sd/status", HTTP_GET, handleStatus);
    server.on("/api/sd/file", HTTP_GET, handleFileDownload);
    server.on("/api/sd/upload", HTTP_POST, handleUploadRequest, handleUploadData);
}

} // namespace SdHandlers
