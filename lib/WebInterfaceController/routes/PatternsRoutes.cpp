/**
 * @file PatternsRoutes.cpp
 * @brief Patterns API endpoint routes
 * @version 260202A
 $12026-02-10
 */
#include "PatternsRoutes.h"
#include "../WebGuiStatus.h"
#include "../WebUtils.h"
#include "Globals.h"
#include "Light/LightRun.h"
#include "Light/PatternCatalog.h"
#include <ArduinoJson.h>
#include <AsyncJson.h>

using WebUtils::sendJson;
using WebUtils::sendError;

namespace PatternsRoutes {

void routeList(AsyncWebServerRequest *request)
{
    String payload;
    String activeId;
    if (!LightRun::patternRead(payload, activeId)) {
        sendError(request, 500, F("Pattern export failed"));
        return;
    }
    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", payload);
    response->addHeader("Cache-Control", "no-store");
    if (!activeId.isEmpty()) {
        response->addHeader("X-Pattern", activeId);
    }
    request->send(response);
}

void routeNext(AsyncWebServerRequest *request)
{
    String error;
    if (LightRun::selectNextPattern(error)) {
        WebGuiStatus::pushState();
        String payload = F("{\"active_pattern\":\"");
        payload += PatternCatalog::instance().activeId();
        payload += F("\"}");
        sendJson(request, payload);
    } else {
        request->send(400, "text/plain", error);
    }
}

void routePrev(AsyncWebServerRequest *request)
{
    String error;
    if (LightRun::selectPrevPattern(error)) {
        WebGuiStatus::pushState();
        String payload = F("{\"active_pattern\":\"");
        payload += PatternCatalog::instance().activeId();
        payload += F("\"}");
        sendJson(request, payload);
    } else {
        request->send(400, "text/plain", error);
    }
}

void attachRoutes(AsyncWebServer &server, AsyncEventSource &events)
{
    server.on("/api/patterns", HTTP_GET, routeList);
    server.on("/api/patterns/next", HTTP_POST, routeNext);
    server.on("/api/patterns/prev", HTTP_POST, routePrev);

    // Select route
    auto *selectRoute = new AsyncCallbackJsonWebHandler("/api/patterns/select");
    selectRoute->onRequest([](AsyncWebServerRequest *request, JsonVariant &json) {
        String error;
        String id;
        JsonObjectConst obj = json.as<JsonObjectConst>();
        if (!obj.isNull() && obj.containsKey("id")) {
            id = obj["id"].as<String>();
        }
        if (id.isEmpty()) {
            if (request->hasParam("id", true)) {
                id = request->getParam("id", true)->value();
            } else if (request->hasParam("id")) {
                id = request->getParam("id")->value();
            }
        }
        PF("[LightRun] HTTP pattern/select id='%s' content-type='%s'\n",
           id.c_str(), request->contentType().c_str());
        if (!LightRun::selectPattern(id, error)) {
            sendError(request, 400, error.isEmpty() ? F("invalid payload") : error);
            return;
        }
        WebGuiStatus::pushState();
        String payload;
        String activeId;
        if (!LightRun::patternRead(payload, activeId)) {
            sendError(request, 500, F("pattern export failed"));
            return;
        }
        sendJson(request, payload, "X-Pattern", activeId);
    });
    selectRoute->setMethod(HTTP_POST);
    server.addHandler(selectRoute);

    // Delete route
    auto *deleteRoute = new AsyncCallbackJsonWebHandler("/api/patterns/delete");
    deleteRoute->onRequest([&events](AsyncWebServerRequest *request, JsonVariant &json) {
        JsonObjectConst obj = json.as<JsonObjectConst>();
        if (obj.isNull()) {
            sendError(request, 400, F("invalid payload"));
            return;
        }
        String affected;
        String error;
        if (!LightRun::deletePattern(obj, affected, error)) {
            sendError(request, 400, error.isEmpty() ? F("invalid payload") : error);
            return;
        }
        String payload;
        String activeId;
        if (!LightRun::patternRead(payload, activeId)) {
            sendError(request, 500, F("pattern export failed"));
            return;
        }
        events.send(payload.c_str(), "patterns", millis());
        const String headerId = affected.length() ? affected : activeId;
        sendJson(request, payload, "X-Pattern", headerId);
    });
    deleteRoute->setMethod(HTTP_POST);
    server.addHandler(deleteRoute);

    // Preview route - MUST be registered BEFORE update route
    auto *previewRoute = new AsyncCallbackJsonWebHandler("/api/patterns/preview", nullptr, 4096);
    previewRoute->setMaxContentLength(2048);
    previewRoute->onRequest([](AsyncWebServerRequest *request, JsonVariant &json) {
        PF("[WebIF] /api/patterns/preview hit\n");
        String error;
        JsonVariantConst body = json;
        if (!LightRun::previewPattern(body, error)) {
            sendError(request, 400, error);
            return;
        }
        sendJson(request, String("{\"status\":\"ok\"}"));
    });
    previewRoute->setMethod(HTTP_POST);
    server.addHandler(previewRoute);

    // Update route (POST to /api/patterns)
    auto *updateRoute = new AsyncCallbackJsonWebHandler("/api/patterns");
    updateRoute->onRequest([](AsyncWebServerRequest *request, JsonVariant &json) {
        JsonObjectConst obj = json.as<JsonObjectConst>();
        if (obj.isNull()) {
            sendError(request, 400, F("invalid payload"));
            return;
        }
        PF("[PatternCatalog] HTTP pattern/update content-type='%s' length=%d\n",
           request->contentType().c_str(), static_cast<int>(request->contentLength()));
        String affected;
        String errorMessage;
        if (!LightRun::updatePattern(obj, affected, errorMessage)) {
            sendError(request, 400, errorMessage.length() ? errorMessage : F("update failed"));
            return;
        }
        String payload;
        String activeId;
        if (!LightRun::patternRead(payload, activeId)) {
            sendError(request, 500, F("pattern export failed"));
            return;
        }
        AsyncWebServerResponse *response = request->beginResponse(200, "application/json", payload);
        response->addHeader("Cache-Control", "no-store");
        const String headerId = affected.length() ? affected : activeId;
        if (!headerId.isEmpty()) {
            response->addHeader("X-Pattern", headerId);
        }
        request->send(response);
    });
    updateRoute->setMethod(HTTP_POST);
    server.addHandler(updateRoute);
}

} // namespace PatternsRoutes