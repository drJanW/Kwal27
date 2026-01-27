/**
 * @file WebDirector.cpp
 * @brief Web request routing and response coordination implementation
 * @version 251231E
 * @date 2025-12-31
 *
 * Implements async web request handling: SD directory listing with JSON
 * chunking, file deletion, pattern/color store operations, and job-based
 * request queuing with TimerManager-driven tick processing.
 */

#include "WebDirector.h"

#include "Globals.h"
#include "TimerManager.h"
#include "SDManager.h"
#include "Light/LightConduct.h"
#include "SdPathUtils.h"

#include <ArduinoJson.h>
#include <SD.h>
#include <cstring>

namespace {

using SdPathUtils::extractBaseName;
using SdPathUtils::parentPath;
using SdPathUtils::sanitizeSdPath;

constexpr uint32_t kJobTickIntervalMs = 5U;
constexpr size_t kMaxSdEntries = 256U;

#if defined(DEBUG_WEB_DIRECTOR)
#define WD_LOG(...) PF(__VA_ARGS__)
#else
#define WD_LOG(...) do {} while (0)
#endif

void sendJsonResponse(AsyncWebServerRequest *request,
                      const String &payload,
                      const String &headerName,
                      const String &headerValue,
                      bool useHeader) {
    if (!request) {
        return;
    }
    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", payload);
    response->addHeader("Cache-Control", "no-store");
    if (useHeader && headerName.length() > 0) {
        response->addHeader(headerName.c_str(), headerValue);
    }
    request->send(response);
}

void sendErrorResponse(AsyncWebServerRequest *request, int code, const String &message) {
    if (!request) {
        return;
    }
    AsyncWebServerResponse *response = request->beginResponse(code, "text/plain", message);
    response->addHeader("Cache-Control", "no-store");
    request->send(response);
}

void appendJsonEscaped(String &out, const char *value) {
    if (!value) {
        return;
    }
    while (*value) {
        const char c = *value++;
        switch (c) {
        case '\\':
            out += F("\\\\");
            break;
        case '"':
            out += F("\\\"");
            break;
        case '\n':
            out += F("\\n");
            break;
        case '\r':
            out += F("\\r");
            break;
        case '\t':
            out += F("\\t");
            break;
        default:
            if (static_cast<unsigned char>(c) < 0x20) {
                break;
            }
            out += c;
            break;
        }
    }
}

} // namespace

WebDirector &WebDirector::instance() {
    static WebDirector inst;
    return inst;
}

void WebDirector::plan() {
    if (!timerStarted_) {
        if (TimerManager::instance().create(kJobTickIntervalMs, 0, cb_webDirectorTick)) {
            timerStarted_ = true;
            WD_LOG("[WebDirector] Job timer started (interval %lu ms)\n", static_cast<unsigned long>(kJobTickIntervalMs));
        } else {
            PL("[WebDirector] Failed to start job timer");
        }
    }
    for (auto &job : jobs_) {
        releaseBusyFlag(job);
        job.reset();
    }
}

bool WebDirector::submitSdList(AsyncWebServerRequest *request, const String &path) {
    if (!request) {
        return false;
    }

    const String sanitizedPath = sanitizeSdPath(path);
    if (sanitizedPath.isEmpty()) {
        sendErrorResponse(request, 400, String(F("Invalid path")));
        return false;
    }

    Job *slot = acquireIdleSlot();
    if (!slot) {
        WD_LOG("[WebDirector] No idle slot for SD list\n");
        return false;
    }

    slot->reset();
    slot->type = Job::Type::SdList;
    slot->state = Job::State::Pending;
    slot->request = request;
    slot->path = sanitizedPath;
    slot->parentPath = parentPath(sanitizedPath);
    slot->entriesBuffer.reserve(256);
    slot->payloadBuffer.reserve(256);
    slot->firstEntry = true;
    slot->headerPrepared = false;

    WD_LOG("[WebDirector] Enqueued SD list for '%s'\n", sanitizedPath.c_str());

    return true;
}

bool WebDirector::submitSdDelete(AsyncWebServerRequest *request, const String &path) {
    if (!request) {
        return false;
    }

    const String sanitizedPath = sanitizeSdPath(path);
    if (sanitizedPath.isEmpty()) {
        sendErrorResponse(request, 400, String(F("Invalid path")));
        return false;
    }

    Job *slot = acquireIdleSlot();
    if (!slot) {
        WD_LOG("[WebDirector] No idle slot for SD delete\n");
        return false;
    }

    slot->reset();
    slot->type = Job::Type::SdDelete;
    slot->state = Job::State::Pending;
    slot->request = request;
    slot->path = sanitizedPath;

    WD_LOG("[WebDirector] Enqueued SD delete for '%s'\n", sanitizedPath.c_str());

    return true;
}

bool WebDirector::submitLightPatternsDump(AsyncWebServerRequest *request) {
    if (!request) {
        return false;
    }

    Job *slot = acquireIdleSlot();
    if (!slot) {
        WD_LOG("[WebDirector] No idle slot for light patterns dump\n");
        return false;
    }

    slot->reset();
    slot->type = Job::Type::LightPatternsDump;
    slot->state = Job::State::Pending;
    slot->request = request;

    WD_LOG("[WebDirector] Enqueued light patterns dump\n");

    return true;
}

bool WebDirector::submitLightColorsDump(AsyncWebServerRequest *request) {
    if (!request) {
        return false;
    }

    Job *slot = acquireIdleSlot();
    if (!slot) {
        WD_LOG("[WebDirector] No idle slot for light colors dump\n");
        return false;
    }

    slot->reset();
    slot->type = Job::Type::LightColorsDump;
    slot->state = Job::State::Pending;
    slot->request = request;

    WD_LOG("[WebDirector] Enqueued light colors dump\n");

    return true;
}

bool WebDirector::submitLightPatternUpdate(AsyncWebServerRequest *request, const String &payload) {
    if (!request) {
        return false;
    }

    Job *slot = acquireIdleSlot();
    if (!slot) {
        WD_LOG("[WebDirector] No idle slot for light pattern update\n");
        return false;
    }

    slot->reset();
    slot->type = Job::Type::LightPatternUpdate;
    slot->state = Job::State::Pending;
    slot->request = request;
    slot->jsonPayload = payload;

    WD_LOG("[WebDirector] Enqueued light pattern update\n");

    return true;
}

bool WebDirector::submitLightPatternDelete(AsyncWebServerRequest *request, const String &payload) {
    if (!request) {
        return false;
    }

    Job *slot = acquireIdleSlot();
    if (!slot) {
        WD_LOG("[WebDirector] No idle slot for light pattern delete\n");
        return false;
    }

    slot->reset();
    slot->type = Job::Type::LightPatternDelete;
    slot->state = Job::State::Pending;
    slot->request = request;
    slot->jsonPayload = payload;

    WD_LOG("[WebDirector] Enqueued light pattern delete\n");

    return true;
}

bool WebDirector::submitLightColorUpdate(AsyncWebServerRequest *request, const String &payload) {
    if (!request) {
        return false;
    }

    Job *slot = acquireIdleSlot();
    if (!slot) {
        WD_LOG("[WebDirector] No idle slot for light color update\n");
        return false;
    }

    slot->reset();
    slot->type = Job::Type::LightColorUpdate;
    slot->state = Job::State::Pending;
    slot->request = request;
    slot->jsonPayload = payload;

    WD_LOG("[WebDirector] Enqueued light color update\n");

    return true;
}

bool WebDirector::submitLightColorDelete(AsyncWebServerRequest *request, const String &payload) {
    if (!request) {
        return false;
    }

    Job *slot = acquireIdleSlot();
    if (!slot) {
        WD_LOG("[WebDirector] No idle slot for light color delete\n");
        return false;
    }

    slot->reset();
    slot->type = Job::Type::LightColorDelete;
    slot->state = Job::State::Pending;
    slot->request = request;
    slot->jsonPayload = payload;

    WD_LOG("[WebDirector] Enqueued light color delete\n");

    return true;
}

void WebDirector::cancelRequest(AsyncWebServerRequest *request) {
    if (!request) {
        return;
    }
    Job *job = findJobByRequest(request);
    if (!job) {
        return;
    }
    job->request = nullptr;
    failJob(*job, 499, String(F("Client disconnected")));
    finalizeJob(*job);
}

void WebDirector::cb_webDirectorTick() {
    WebDirector::instance().processJobs();
}

void WebDirector::processJobs() {
    for (auto &job : jobs_) {
        switch (job.state) {
        case Job::State::Pending:
            WD_LOG("[WebDirector] Starting job type %d\n", static_cast<int>(job.type));
            startJob(job);
            break;
        case Job::State::Running:
            switch (job.type) {
            case Job::Type::SdList:
                runSdListJob(job);
                break;
            case Job::Type::SdDelete:
                runSdDeleteJob(job);
                break;
            case Job::Type::LightPatternsDump:
                runLightPatternsGetJob(job);
                break;
            case Job::Type::LightColorsDump:
                runLightColorsGetJob(job);
                break;
            case Job::Type::LightPatternUpdate:
                runLightPatternUpdateJob(job);
                break;
            case Job::Type::LightPatternDelete:
                runLightPatternDeleteJob(job);
                break;
            case Job::Type::LightColorUpdate:
                runLightColorUpdateJob(job);
                break;
            case Job::Type::LightColorDelete:
                runLightColorDeleteJob(job);
                break;
            }
            break;
        case Job::State::Finishing:
        case Job::State::Failed:
            WD_LOG("[WebDirector] Finalizing job type %d state %d\n",
                   static_cast<int>(job.type),
                   static_cast<int>(job.state));
            finalizeJob(job);
            break;
        default:
            break;
        }
    }
}

bool WebDirector::tryAcquireBusyFlag(Job &job) {
    if (job.sdBusyClaimed) {
        return true;
    }

    if (SDManager::isSDbusy()) {
        return false;
    }
    SDManager::setSDbusy(true);
    job.sdBusyClaimed = true;
    return true;
}

void WebDirector::releaseBusyFlag(Job &job) {
    if (job.sdBusyClaimed) {
        SDManager::setSDbusy(false);
        job.sdBusyClaimed = false;
    }
}

void WebDirector::startJob(Job &job) {
    if (job.state != Job::State::Pending) {
        return;
    }

    switch (job.type) {
    case Job::Type::SdList:
        if (job.sdBusyClaimed && !job.dirOpen) {
            job.state = Job::State::Failed;
            job.statusCode = 500;
            job.errorMessage = String(F("SD busy state invalid"));
            return;
        }

        if (!tryAcquireBusyFlag(job)) {
            return;
        }

        job.dirHandle = SD.open(job.path.c_str(), FILE_READ);
        if (!job.dirHandle) {
            failJob(job, 404, String(F("Path not found")));
            return;
        }
        if (!job.dirHandle.isDirectory()) {
            failJob(job, 400, String(F("Not a directory")));
            return;
        }

        job.dirOpen = true;
        job.state = Job::State::Running;
        WD_LOG("[WebDirector] SD list started '%s'\n", job.path.c_str());
        break;
    case Job::Type::SdDelete: {
        if (job.path.length() == 0 || job.path == "/") {
            failJob(job, 400, String(F("Invalid path")));
            return;
        }

        if (!tryAcquireBusyFlag(job)) {
            return;
        }

        if (!SD.exists(job.path.c_str())) {
            failJob(job, 404, String(F("Path not found")));
            return;
        }

        job.sdDeleteStack.clear();
        Job::SdDeleteEntry rootEntry;
        rootEntry.path = job.path;
        rootEntry.postRemoval = false;
        job.sdDeleteStack.push_back(rootEntry);
        job.payloadBuffer = String();
        job.state = Job::State::Running;
        WD_LOG("[WebDirector] SD delete started '%s'\n", job.path.c_str());
        break;
    }
    case Job::Type::LightPatternsDump:
    case Job::Type::LightColorsDump:
    case Job::Type::LightPatternUpdate:
    case Job::Type::LightPatternDelete:
    case Job::Type::LightColorUpdate:
    case Job::Type::LightColorDelete:
        job.state = Job::State::Running;
        break;
    }
}

void WebDirector::runSdListJob(Job &job) {
    if (job.state != Job::State::Running) {
        return;
    }

    size_t processed = 0;
    while (processed < kSdEntriesPerSlice) {
        File entry = job.dirHandle.openNextFile();
        if (!entry) {
            job.state = Job::State::Finishing;
            break;
        }

        if (job.entryCount >= kMaxSdEntries) {
            job.truncated = true;
            entry.close();
            job.state = Job::State::Finishing;
            break;
        }

        const bool isDir = entry.isDirectory();
        const uint32_t sizeBytes = isDir ? 0U : static_cast<uint32_t>(entry.size());
    const String baseName = extractBaseName(entry.name());

        if (!job.firstEntry) {
            job.entriesBuffer += ',';
        }
        job.firstEntry = false;

        job.entriesBuffer += F("{\"name\":\"");
    appendJsonEscaped(job.entriesBuffer, baseName.c_str());
        job.entriesBuffer += F("\",\"type\":\"");
        job.entriesBuffer += isDir ? F("dir") : F("file");
        job.entriesBuffer += F("\",\"size\":");
        job.entriesBuffer += String(sizeBytes);
        job.entriesBuffer += F("}");

        ++job.entryCount;
        ++processed;

        entry.close();
    }
}

void WebDirector::runSdDeleteJob(Job &job) {
    if (job.state != Job::State::Running) {
        return;
    }

    size_t processed = 0;
    while (processed < kSdDeleteStepsPerSlice) {
        if (job.sdDeleteStack.empty()) {
            if (job.payloadBuffer.isEmpty()) {
                job.payloadBuffer = F("{\"status\":\"ok\"}");
            }
            job.state = Job::State::Finishing;
            break;
        }

        Job::SdDeleteEntry entry = job.sdDeleteStack.back();
        job.sdDeleteStack.pop_back();
        const String &targetPath = entry.path;

        if (entry.postRemoval) {
            if (!SD.rmdir(targetPath.c_str())) {
                failJob(job, 500, String(F("Remove directory failed")));
                return;
            }
            ++processed;
            continue;
        }

        File node = SD.open(targetPath.c_str(), FILE_READ);
        if (!node) {
            failJob(job, 404, String(F("Path not found during delete")));
            return;
        }
        const bool isDir = node.isDirectory();
        node.close();

        if (!isDir) {
            if (!SD.remove(targetPath.c_str())) {
                failJob(job, 500, String(F("Delete failed")));
                return;
            }
            ++processed;
            continue;
        }

    Job::SdDeleteEntry dirEntry;
    dirEntry.path = targetPath;
    dirEntry.postRemoval = true;
    job.sdDeleteStack.push_back(dirEntry);

        File dir = SD.open(targetPath.c_str(), FILE_READ);
        if (!dir) {
            failJob(job, 500, String(F("Dir open failed")));
            return;
        }
        for (File child = dir.openNextFile(); child; child = dir.openNextFile()) {
            const char *childName = child.name();
            String childPath = targetPath;
            if (!childPath.endsWith("/")) {
                childPath += '/';
            }
            if (childName) {
                childPath += extractBaseName(childName);
            }
            Job::SdDeleteEntry childEntry;
            childEntry.path = childPath;
            childEntry.postRemoval = false;
            job.sdDeleteStack.push_back(childEntry);
            child.close();
        }
        dir.close();
        ++processed;
    }

    if (job.state == Job::State::Running && job.sdDeleteStack.empty()) {
        job.payloadBuffer = F("{\"status\":\"ok\"}");
        job.state = Job::State::Finishing;
    }
}

void WebDirector::runLightPatternsGetJob(Job &job) {
    if (job.state != Job::State::Running) {
        return;
    }

    String activeId;
    if (!LightConduct::patternRead(job.payloadBuffer, activeId)) {
        PF("[WebDirector] LightPatternsDump: empty payload\n");
        failJob(job, 500, String(F("Pattern export failed")));
        return;
    }

    job.headerName = F("X-Pattern");
    job.headerValue = activeId;
    job.useHeader = true;
    const unsigned patternBytes = static_cast<unsigned>(job.payloadBuffer.length());
    const unsigned patternPreviewLen = patternBytes > 120U ? 120U : patternBytes;
    String patternPreview = job.payloadBuffer.substring(0, patternPreviewLen);
    PF("[WebDirector] LightPatternsDump: bytes=%u active='%s' preview='%s'%s\n",
       patternBytes,
       job.headerValue.c_str(),
       patternPreview.c_str(),
       patternBytes > patternPreviewLen ? "…" : "");
    job.state = Job::State::Finishing;
}

void WebDirector::runLightColorsGetJob(Job &job) {
    if (job.state != Job::State::Running) {
        return;
    }

    String activeId;
    if (!LightConduct::colorRead(job.payloadBuffer, activeId)) {
        PF("[WebDirector] LightColorsDump: empty payload\n");
        failJob(job, 500, String(F("Color export failed")));
        return;
    }

    job.headerName = F("X-Color");
    job.headerValue = activeId;
    job.useHeader = true;
    const unsigned colorBytes = static_cast<unsigned>(job.payloadBuffer.length());
    const unsigned colorPreviewLen = colorBytes > 120U ? 120U : colorBytes;
    String colorPreview = job.payloadBuffer.substring(0, colorPreviewLen);
    PF("[WebDirector] LightColorsDump: bytes=%u active='%s' preview='%s'%s\n",
       colorBytes,
       job.headerValue.c_str(),
       colorPreview.c_str(),
       colorBytes > colorPreviewLen ? "…" : "");
    job.state = Job::State::Finishing;
}

void WebDirector::runLightPatternUpdateJob(Job &job) {
    if (job.state != Job::State::Running) {
        return;
    }

    if (job.jsonPayload.length() == 0) {
        failJob(job, 400, String(F("Payload missing")));
        return;
    }

    DynamicJsonDocument doc(4096);
    DeserializationError err = deserializeJson(doc, job.jsonPayload);
    if (err) {
        failJob(job, 400, String(F("Invalid JSON")));
        return;
    }

    String affected;
    String errorMessage;
    JsonVariantConst body = doc.as<JsonVariantConst>();
    if (!LightConduct::updatePattern(body, affected, errorMessage)) {
        if (errorMessage.length() == 0) {
            errorMessage = F("Update failed");
        }
        failJob(job, 400, errorMessage);
        return;
    }

    String activeId;
    if (!LightConduct::patternRead(job.payloadBuffer, activeId)) {
        failJob(job, 500, String(F("Pattern export failed")));
        return;
    }

    job.headerName = F("X-Pattern");
    job.headerValue = affected.length() ? affected : activeId;
    job.useHeader = true;
    job.jsonPayload = String();
    job.state = Job::State::Finishing;
}

void WebDirector::runLightPatternDeleteJob(Job &job) {
    if (job.state != Job::State::Running) {
        return;
    }

    if (job.jsonPayload.length() == 0) {
        failJob(job, 400, String(F("Payload missing")));
        return;
    }

    DynamicJsonDocument doc(2048);
    DeserializationError err = deserializeJson(doc, job.jsonPayload);
    if (err) {
        failJob(job, 400, String(F("Invalid JSON")));
        return;
    }

    String affected;
    String errorMessage;
    JsonVariantConst body = doc.as<JsonVariantConst>();
    if (!LightConduct::deletePattern(body, affected, errorMessage)) {
        if (errorMessage.length() == 0) {
            errorMessage = F("Delete failed");
        }
        failJob(job, 400, errorMessage);
        return;
    }

    String activeId;
    if (!LightConduct::patternRead(job.payloadBuffer, activeId)) {
        failJob(job, 500, String(F("Pattern export failed")));
        return;
    }

    job.headerName = F("X-Pattern");
    job.headerValue = affected.length() ? affected : activeId;
    job.useHeader = true;
    job.jsonPayload = String();
    job.state = Job::State::Finishing;
}

void WebDirector::runLightColorUpdateJob(Job &job) {
    if (job.state != Job::State::Running) {
        return;
    }

    if (job.jsonPayload.length() == 0) {
        failJob(job, 400, String(F("Payload missing")));
        return;
    }

    DynamicJsonDocument doc(4096);
    DeserializationError err = deserializeJson(doc, job.jsonPayload);
    if (err) {
        failJob(job, 400, String(F("Invalid JSON")));
        return;
    }

    String affected;
    String errorMessage;
    JsonVariantConst body = doc.as<JsonVariantConst>();
    if (!LightConduct::updateColor(body, affected, errorMessage)) {
        if (errorMessage.length() == 0) {
            errorMessage = F("Update failed");
        }
        failJob(job, 400, errorMessage);
        return;
    }

    String activeId;
    if (!LightConduct::colorRead(job.payloadBuffer, activeId)) {
        failJob(job, 500, String(F("Color export failed")));
        return;
    }

    job.headerName = F("X-Color");
    job.headerValue = affected.length() ? affected : activeId;
    job.useHeader = true;
    job.jsonPayload = String();
    job.state = Job::State::Finishing;
}

void WebDirector::runLightColorDeleteJob(Job &job) {
    if (job.state != Job::State::Running) {
        return;
    }

    if (job.jsonPayload.length() == 0) {
        failJob(job, 400, String(F("Payload missing")));
        return;
    }

    DynamicJsonDocument doc(2048);
    DeserializationError err = deserializeJson(doc, job.jsonPayload);
    if (err) {
        failJob(job, 400, String(F("Invalid JSON")));
        return;
    }

    String affected;
    String errorMessage;
    JsonVariantConst body = doc.as<JsonVariantConst>();
    if (!LightConduct::deleteColorSet(body, affected, errorMessage)) {
        if (errorMessage.length() == 0) {
            errorMessage = F("Delete failed");
        }
        failJob(job, 400, errorMessage);
        return;
    }

    String activeId;
    if (!LightConduct::colorRead(job.payloadBuffer, activeId)) {
        failJob(job, 500, String(F("Color export failed")));
        return;
    }

    job.headerName = F("X-Color");
    job.headerValue = affected.length() ? affected : activeId;
    job.useHeader = true;
    job.jsonPayload = String();
    job.state = Job::State::Finishing;
}

void WebDirector::finalizeJob(Job &job) {
    if (job.dirOpen) {
        job.dirHandle.close();
        job.dirOpen = false;
    }

    releaseBusyFlag(job);

    if (!job.request) {
        releaseJob(job);
        return;
    }

    if (job.state == Job::State::Failed) {
        String message = job.errorMessage.length() ? job.errorMessage : String(F("Job failed"));
        sendErrorResponse(job.request, job.statusCode, message);
        releaseJob(job);
        return;
    }

    if (job.type == Job::Type::SdList) {
        if (!job.headerPrepared) {
            job.payloadBuffer.reserve(128 + job.entriesBuffer.length());
            job.payloadBuffer += F("{\"path\":\"");
            appendJsonEscaped(job.payloadBuffer, job.path.c_str());
            job.payloadBuffer += F("\",\"parent\":\"");
            appendJsonEscaped(job.payloadBuffer, job.parentPath.c_str());
            job.payloadBuffer += F("\",\"ready\":true,\"busy\":");
            job.payloadBuffer += SDManager::isSDbusy() ? F("true") : F("false");
            job.payloadBuffer += F(",\"entryCount\":");
            job.payloadBuffer += String(job.entryCount);
            job.payloadBuffer += F(",\"truncated\":");
            job.payloadBuffer += job.truncated ? F("true") : F("false");
            job.payloadBuffer += F(",\"entries\":[");
            job.payloadBuffer += job.entriesBuffer;
            job.payloadBuffer += F("]}");
            job.headerPrepared = true;
        }
    }

    if (job.type == Job::Type::SdDelete && job.payloadBuffer.isEmpty()) {
        job.payloadBuffer = F("{\"status\":\"ok\"}");
    }

    sendJsonResponse(job.request, job.payloadBuffer, job.headerName, job.headerValue, job.useHeader);
    releaseJob(job);
}

void WebDirector::failJob(Job &job, int statusCode, const String &message) {
    job.statusCode = statusCode;
    job.errorMessage = message;
    job.useHeader = false;
    job.headerName = String();
    job.headerValue = String();
    job.state = Job::State::Failed;
}

void WebDirector::releaseJob(Job &job) {
    if (job.dirOpen) {
        job.dirHandle.close();
        job.dirOpen = false;
    }
    releaseBusyFlag(job);
    job.reset();
}

WebDirector::Job *WebDirector::findJobByRequest(AsyncWebServerRequest *request) {
    for (auto &job : jobs_) {
        if (job.request == request) {
            return &job;
        }
    }
    return nullptr;
}

WebDirector::Job *WebDirector::acquireIdleSlot() {
    for (auto &job : jobs_) {
        if (job.state == Job::State::Idle) {
            return &job;
        }
    }
    return nullptr;
}

void WebDirector::Job::reset() {
    state = State::Idle;
    request = nullptr;
    path = String();
    parentPath = String();
    payloadBuffer = String();
    entriesBuffer = String();
    jsonPayload = String();
    useHeader = false;
    headerName = String();
    headerValue = String();
    entryCount = 0;
    truncated = false;
    statusCode = 200;
    errorMessage = String();
    dirHandle = File();
    dirOpen = false;
    firstEntry = true;
    headerPrepared = false;
    sdDeleteStack.clear();
}
