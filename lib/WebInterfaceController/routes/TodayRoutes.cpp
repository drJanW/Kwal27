/**
 * @file TodayRoutes.cpp
 * @brief Today API endpoint routes
 * @version 260202A
 $12026-02-05
 */
#include "TodayRoutes.h"
#include "../WebUtils.h"
#include "Globals.h"
#include "TodayState.h"
#include "Calendar/CalendarRun.h"
#include <ArduinoJson.h>

using WebUtils::sendJson;
using WebUtils::sendError;
using WebUtils::rgbToHex;
using WebUtils::toIdString;

namespace TodayRoutes {

void routeToday(AsyncWebServerRequest *request)
{
    TodayState ctx;
    if (!calendarRun.todayRead(ctx)) {
        sendError(request, 503, F("Today data unavailable"));
        return;
    }

    const bool calendarManaged = ctx.entry.valid;

    DynamicJsonDocument doc(1024);
    doc["valid"] = ctx.valid;
    if (ctx.entry.iso.length()) {
        doc["date_iso"] = ctx.entry.iso;
    }
    doc["calendar_entry"] = ctx.entry.valid;
    if (ctx.entry.note.length()) {
        doc["note"] = ctx.entry.note;
    }

    JsonObject patternObj = doc.createNestedObject("pattern");
    if (ctx.pattern.valid) {
        const String resolvedId = toIdString(ctx.pattern.id);
        if (resolvedId.length()) {
            patternObj["id"] = resolvedId;
        }
        if (ctx.pattern.label.length()) {
            patternObj["label"] = ctx.pattern.label;
        }
    }
    if (ctx.entry.patternId != 0) {
        patternObj["calendar_id"] = static_cast<uint32_t>(ctx.entry.patternId);
    }
    patternObj["source"] = calendarManaged ? "calendar" : "context";

    JsonObject colorObj = doc.createNestedObject("color");
    if (ctx.colors.valid) {
        const String resolvedId = toIdString(ctx.colors.id);
        if (resolvedId.length()) {
            colorObj["id"] = resolvedId;
        }
        if (ctx.colors.label.length()) {
            colorObj["label"] = ctx.colors.label;
        }
        const String colorAHex = rgbToHex(ctx.colors.colorA.r, ctx.colors.colorA.g, ctx.colors.colorA.b);
        colorObj["colorA_hex"] = colorAHex;
        colorObj["rgb1_hex"] = colorAHex; // legacy alias
        const String colorBHex = rgbToHex(ctx.colors.colorB.r, ctx.colors.colorB.g, ctx.colors.colorB.b);
        colorObj["colorB_hex"] = colorBHex;
        colorObj["rgb2_hex"] = colorBHex; // legacy alias
    }
    if (ctx.entry.colorId != 0) {
        colorObj["calendar_id"] = static_cast<uint32_t>(ctx.entry.colorId);
    }
    colorObj["source"] = calendarManaged ? "calendar" : "context";

    String payload;
    serializeJson(doc, payload);
    sendJson(request, payload);
}

void attachRoutes(AsyncWebServer &server)
{
    server.on("/api/context/today", HTTP_GET, routeToday);
}

} // namespace TodayRoutes