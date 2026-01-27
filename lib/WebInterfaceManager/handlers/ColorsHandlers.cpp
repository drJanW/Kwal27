/**
 * @file ColorsHandlers.cpp
 * @brief Colors API endpoint handlers
 * @version 251231E
 * @date 2025-12-31
 *
 * Implements HTTP handlers for the /api/colors/* endpoints.
 * Provides routes to list available color schemes, navigate to
 * next/previous colors, and manage active color selection.
 * Integrates with LightConduct and ColorsStore for color control.
 */

#include "ColorsHandlers.h"
#include "../WebGuiStatus.h"
#include "../WebUtils.h"
#include "Globals.h"
#include "Light/LightConduct.h"
#include "Light/ColorsStore.h"
#include <ArduinoJson.h>
#include <AsyncJson.h>

using WebUtils::sendJson;
using WebUtils::sendError;

namespace ColorsHandlers {

void handleList(AsyncWebServerRequest *request)
{
    String payload;
    String activeId;
    if (!LightConduct::colorRead(payload, activeId)) {
        sendError(request, 500, F("Color export failed"));
        return;
    }
    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", payload);
    response->addHeader("Cache-Control", "no-store");
    if (!activeId.isEmpty()) {
        response->addHeader("X-Color", activeId);
    }
    request->send(response);
}

void handleNext(AsyncWebServerRequest *request)
{
    String error;
    if (LightConduct::selectNextColor(error)) {
        WebGuiStatus::pushState();
        String payload = F("{\"active_color\":\"");
        payload += ColorsStore::instance().getActiveColorId();
        payload += F("\"}");
        sendJson(request, payload);
    } else {
        request->send(400, "text/plain", error);
    }
}

void handlePrev(AsyncWebServerRequest *request)
{
    String error;
    if (LightConduct::selectPrevColor(error)) {
        WebGuiStatus::pushState();
        String payload = F("{\"active_color\":\"");
        payload += ColorsStore::instance().getActiveColorId();
        payload += F("\"}");
        sendJson(request, payload);
    } else {
        request->send(400, "text/plain", error);
    }
}

void attachRoutes(AsyncWebServer &server, AsyncEventSource &events)
{
    server.on("/api/colors", HTTP_GET, handleList);
    server.on("/api/colors/next", HTTP_POST, handleNext);
    server.on("/api/colors/prev", HTTP_POST, handlePrev);

    // Select handler
    auto *selectHandler = new AsyncCallbackJsonWebHandler("/api/colors/select");
    selectHandler->onRequest([](AsyncWebServerRequest *request, JsonVariant &json) {
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
        PF("[LightConduct] HTTP color/select id='%s' content-type='%s'\n",
           id.c_str(), request->contentType().c_str());
        if (!LightConduct::selectColor(id, error)) {
            sendError(request, 400, error.isEmpty() ? F("invalid payload") : error);
            return;
        }
        WebGuiStatus::pushState();
        String payload;
        String activeId;
        if (!LightConduct::colorRead(payload, activeId)) {
            sendError(request, 500, F("color export failed"));
            return;
        }
        sendJson(request, payload, "X-Color", activeId);
    });
    selectHandler->setMethod(HTTP_POST);
    server.addHandler(selectHandler);

    // Delete handler
    auto *deleteHandler = new AsyncCallbackJsonWebHandler("/api/colors/delete");
    deleteHandler->onRequest([&events](AsyncWebServerRequest *request, JsonVariant &json) {
        JsonObjectConst obj = json.as<JsonObjectConst>();
        if (obj.isNull()) {
            sendError(request, 400, F("invalid payload"));
            return;
        }
        String affected;
        String error;
        if (!LightConduct::deleteColorSet(obj, affected, error)) {
            sendError(request, 400, error.isEmpty() ? F("invalid payload") : error);
            return;
        }
        String payload;
        String activeId;
        if (!LightConduct::colorRead(payload, activeId)) {
            sendError(request, 500, F("color export failed"));
            return;
        }
        events.send(payload.c_str(), "colors", millis());
        const String headerId = affected.length() ? affected : activeId;
        sendJson(request, payload, "X-Color", headerId);
    });
    deleteHandler->setMethod(HTTP_POST);
    server.addHandler(deleteHandler);

    // Preview handler - MUST be registered BEFORE update handler
    auto *previewHandler = new AsyncCallbackJsonWebHandler("/api/colors/preview", nullptr, 2048);
    previewHandler->setMaxContentLength(1024);
    previewHandler->onRequest([](AsyncWebServerRequest *request, JsonVariant &json) {
        PF("[WebIF] /api/colors/preview hit\n");
        String error;
        JsonVariantConst body = json;
        if (!LightConduct::previewColor(body, error)) {
            sendError(request, 400, error.isEmpty() ? F("invalid payload") : error);
            return;
        }
        sendJson(request, String("{\"status\":\"ok\"}"));
    });
    previewHandler->setMethod(HTTP_POST);
    server.addHandler(previewHandler);

    // Update handler (POST to /api/colors)
    auto *updateHandler = new AsyncCallbackJsonWebHandler("/api/colors");
    updateHandler->onRequest([](AsyncWebServerRequest *request, JsonVariant &json) {
        JsonObjectConst obj = json.as<JsonObjectConst>();
        if (obj.isNull()) {
            sendError(request, 400, F("invalid payload"));
            return;
        }
        const String remoteIp = request->client() ? request->client()->remoteIP().toString() : String(F("unknown"));
        PF("[LightConduct] HTTP colors/update from %s content-type='%s' length=%d\n",
           remoteIp.c_str(), request->contentType().c_str(), static_cast<int>(request->contentLength()));
        String affected;
        String errorMessage;
        if (!LightConduct::updateColor(obj, affected, errorMessage)) {
            sendError(request, 400, errorMessage.length() ? errorMessage : F("update failed"));
            return;
        }
        String payload;
        String activeId;
        if (!LightConduct::colorRead(payload, activeId)) {
            sendError(request, 500, F("color export failed"));
            return;
        }
        AsyncWebServerResponse *response = request->beginResponse(200, "application/json", payload);
        response->addHeader("Cache-Control", "no-store");
        const String headerId = affected.length() ? affected : activeId;
        if (!headerId.isEmpty()) {
            response->addHeader("X-Color", headerId);
        }
        request->send(response);
    });
    updateHandler->setMethod(HTTP_POST);
    server.addHandler(updateHandler);
}

} // namespace ColorsHandlers
