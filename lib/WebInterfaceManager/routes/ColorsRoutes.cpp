/**
 * @file ColorsRoutes.cpp
 * @brief Colors API endpoint routes
 * @version 251231E
 * @date 2025-12-31
 *
 * Implements HTTP routes for the /api/colors/* endpoints.
 * Provides routes to list available color schemes, navigate to
 * next/previous colors, and manage active color selection.
 * Integrates with LightRun and ColorsCatalog for color control.
 */

#include "ColorsRoutes.h"
#include "../WebGuiStatus.h"
#include "../WebUtils.h"
#include "Globals.h"
#include "Light/LightRun.h"
#include "Light/ColorsCatalog.h"
#include <ArduinoJson.h>
#include <AsyncJson.h>

using WebUtils::sendJson;
using WebUtils::sendError;

namespace ColorsRoutes {

void routeList(AsyncWebServerRequest *request)
{
    String payload;
    String activeId;
    if (!LightRun::colorRead(payload, activeId)) {
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

void routeNext(AsyncWebServerRequest *request)
{
    String error;
    if (LightRun::selectNextColor(error)) {
        WebGuiStatus::pushState();
        String payload = F("{\"active_color\":\"");
        payload += ColorsCatalog::instance().getActiveColorId();
        payload += F("\"}");
        sendJson(request, payload);
    } else {
        request->send(400, "text/plain", error);
    }
}

void routePrev(AsyncWebServerRequest *request)
{
    String error;
    if (LightRun::selectPrevColor(error)) {
        WebGuiStatus::pushState();
        String payload = F("{\"active_color\":\"");
        payload += ColorsCatalog::instance().getActiveColorId();
        payload += F("\"}");
        sendJson(request, payload);
    } else {
        request->send(400, "text/plain", error);
    }
}

void attachRoutes(AsyncWebServer &server, AsyncEventSource &events)
{
    server.on("/api/colors", HTTP_GET, routeList);
    server.on("/api/colors/next", HTTP_POST, routeNext);
    server.on("/api/colors/prev", HTTP_POST, routePrev);

    // Select route
    auto *selectRoute = new AsyncCallbackJsonWebHandler("/api/colors/select");
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
        PF("[LightRun] HTTP color/select id='%s' content-type='%s'\n",
           id.c_str(), request->contentType().c_str());
        if (!LightRun::selectColor(id, error)) {
            sendError(request, 400, error.isEmpty() ? F("invalid payload") : error);
            return;
        }
        WebGuiStatus::pushState();
        String payload;
        String activeId;
        if (!LightRun::colorRead(payload, activeId)) {
            sendError(request, 500, F("color export failed"));
            return;
        }
        sendJson(request, payload, "X-Color", activeId);
    });
    selectRoute->setMethod(HTTP_POST);
    server.addHandler(selectRoute);

    // Delete route
    auto *deleteRoute = new AsyncCallbackJsonWebHandler("/api/colors/delete");
    deleteRoute->onRequest([&events](AsyncWebServerRequest *request, JsonVariant &json) {
        JsonObjectConst obj = json.as<JsonObjectConst>();
        if (obj.isNull()) {
            sendError(request, 400, F("invalid payload"));
            return;
        }
        String affected;
        String error;
        if (!LightRun::deleteColorSet(obj, affected, error)) {
            sendError(request, 400, error.isEmpty() ? F("invalid payload") : error);
            return;
        }
        String payload;
        String activeId;
        if (!LightRun::colorRead(payload, activeId)) {
            sendError(request, 500, F("color export failed"));
            return;
        }
        events.send(payload.c_str(), "colors", millis());
        const String headerId = affected.length() ? affected : activeId;
        sendJson(request, payload, "X-Color", headerId);
    });
    deleteRoute->setMethod(HTTP_POST);
    server.addHandler(deleteRoute);

    // Preview route - MUST be registered BEFORE update route
    auto *previewRoute = new AsyncCallbackJsonWebHandler("/api/colors/preview", nullptr, 2048);
    previewRoute->setMaxContentLength(1024);
    previewRoute->onRequest([](AsyncWebServerRequest *request, JsonVariant &json) {
        PF("[WebIF] /api/colors/preview hit\n");
        String error;
        JsonVariantConst body = json;
        if (!LightRun::previewColor(body, error)) {
            sendError(request, 400, error.isEmpty() ? F("invalid payload") : error);
            return;
        }
        sendJson(request, String("{\"status\":\"ok\"}"));
    });
    previewRoute->setMethod(HTTP_POST);
    server.addHandler(previewRoute);

    // Update route (POST to /api/colors)
    auto *updateRoute = new AsyncCallbackJsonWebHandler("/api/colors");
    updateRoute->onRequest([](AsyncWebServerRequest *request, JsonVariant &json) {
        JsonObjectConst obj = json.as<JsonObjectConst>();
        if (obj.isNull()) {
            sendError(request, 400, F("invalid payload"));
            return;
        }
        const String remoteIp = request->client() ? request->client()->remoteIP().toString() : String(F("unknown"));
        PF("[LightRun] HTTP colors/update from %s content-type='%s' length=%d\n",
           remoteIp.c_str(), request->contentType().c_str(), static_cast<int>(request->contentLength()));
        String affected;
        String errorMessage;
        if (!LightRun::updateColor(obj, affected, errorMessage)) {
            sendError(request, 400, errorMessage.length() ? errorMessage : F("update failed"));
            return;
        }
        String payload;
        String activeId;
        if (!LightRun::colorRead(payload, activeId)) {
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
    updateRoute->setMethod(HTTP_POST);
    server.addHandler(updateRoute);
}

} // namespace ColorsRoutes