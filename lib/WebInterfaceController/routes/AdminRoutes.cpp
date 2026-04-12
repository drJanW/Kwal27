/**
 * @file AdminRoutes.cpp
 * @brief Admin settings API endpoint routes (globals.csv editor)
 * @version 260412A
 * @date 2026-04-12
 */
#include <Arduino.h>
#include "AdminRoutes.h"
#include "../WebUtils.h"
#include "Globals.h"
#include "SDController.h"
#include "Alert/AlertState.h"
#include <AsyncJson.h>
#include <ArduinoJson.h>
#include <SD.h>

using WebUtils::sendJson;
using WebUtils::sendError;
using WebUtils::appendJsonEscaped;

namespace AdminRoutes {

// ── PIN check helper ──────────────────────────────────────────────
static bool checkPin(AsyncWebServerRequest *request) {
    if (!request->hasParam("pin")) return false;
    uint16_t pin = request->getParam("pin")->value().toInt();
    return pin == Globals::wifiConfigPin;
}

// ── Section header detector ───────────────────────────────────────
// Matches lines like: # ═══ SECTION NAME ═══
static bool isSectionHeader(const char *line) {
    if (line[0] != '#') return false;
    // Look for ═ (UTF-8: 0xE2 0x95 0x90) in the line
    return strstr(line, "\xE2\x95\x90") != nullptr;
}

// Extract section name from header like "# ═══ SECTION NAME ═══"
static String extractSectionName(const char *line) {
    const char *start = line;
    // Skip leading # and whitespace
    while (*start == '#' || *start == ' ') start++;
    // Find first non-═ character after the leading ═══
    // ═ is 3 bytes: E2 95 90
    while (*start == '\xE2' && *(start+1) == '\x95' && *(start+2) == '\x90') start += 3;
    while (*start == ' ') start++;

    String name;
    const char *end = start;
    // Read until next ═ or end of line
    while (*end && !(*end == '\xE2' && *(end+1) == '\x95' && *(end+2) == '\x90') && *end != '\n' && *end != '\r') {
        end++;
    }
    // Trim trailing spaces
    while (end > start && *(end-1) == ' ') end--;
    name = String(start).substring(0, end - start);
    return name;
}

// ── GET /api/admin/globals ────────────────────────────────────────
// Reads globals.csv from SD and returns JSON array of entries.
// Section headers → {"section":"NAME"}
// Data lines → {"key":"k","type":"t","value":"v","comment":"c","active":true/false}
// Commented data lines (starting with #) → active:false, # stripped from key
static void routeGetGlobals(AsyncWebServerRequest *request) {
    if (!checkPin(request)) {
        request->send(403);
        return;
    }
    if (!AlertState::isSdOk()) {
        sendError(request, 503, F("SD not ready"));
        return;
    }
    if (AlertState::isSdBusy()) {
        sendError(request, 409, F("SD busy"));
        return;
    }

    SDController::lockSD();
    File file = SD.open("/globals.csv", FILE_READ);  // NOCHECK: accepted, guarded by lockSD
    if (!file) {
        SDController::unlockSD();
        sendError(request, 404, F("globals.csv not found"));
        return;
    }

    String json = "[";
    bool first = true;
    char lineBuf[256];

    while (file.available()) {
        // Read one line
        int idx = 0;
        while (file.available() && idx < (int)sizeof(lineBuf) - 1) {
            char c = file.read();  // NOCHECK: accepted, guarded by lockSD
            if (c == '\n') break;
            if (c != '\r') lineBuf[idx++] = c;
        }
        lineBuf[idx] = '\0';

        // Skip empty lines
        if (idx == 0) continue;

        // Skip pure comment lines (format header, explanatory text)
        // but keep section headers and commented-out parameters
        if (lineBuf[0] == '/' && lineBuf[1] == '/') continue;

        // Section header?
        if (isSectionHeader(lineBuf)) {
            if (!first) json += ',';
            first = false;
            String name = extractSectionName(lineBuf);
            json += F("{\"section\":\"");
            appendJsonEscaped(json, name.c_str());
            json += F("\"}");
            continue;
        }

        // Plain comment (not a section header, not a data line)
        // Check if it's a commented-out parameter: #key;type;value;comment
        const char *dataStart = lineBuf;
        bool active = true;
        if (lineBuf[0] == '#') {
            dataStart = lineBuf + 1;
            // Check if it looks like a parameter (has semicolons)
            if (!strchr(dataStart, ';')) continue;  // Plain comment, skip
            active = false;
        }

        // Parse: key;type;value;comment
        // Find semicolons
        const char *semi1 = strchr(dataStart, ';');
        if (!semi1) continue;
        const char *semi2 = strchr(semi1 + 1, ';');
        if (!semi2) continue;
        const char *semi3 = strchr(semi2 + 1, ';');

        String key = String(dataStart).substring(0, semi1 - dataStart);
        String type = String(semi1 + 1).substring(0, semi2 - semi1 - 1);
        String value;
        String comment;
        if (semi3) {
            value = String(semi2 + 1).substring(0, semi3 - semi2 - 1);
            comment = String(semi3 + 1);
        } else {
            value = String(semi2 + 1);
        }

        key.trim();
        type.trim();
        value.trim();
        comment.trim();

        if (key.isEmpty() || type.isEmpty()) continue;

        if (!first) json += ',';
        first = false;
        json += F("{\"key\":\"");
        appendJsonEscaped(json, key.c_str());
        json += F("\",\"type\":\"");
        appendJsonEscaped(json, type.c_str());
        json += F("\",\"value\":\"");
        appendJsonEscaped(json, value.c_str());
        json += F("\",\"comment\":\"");
        appendJsonEscaped(json, comment.c_str());
        json += F("\",\"active\":");
        json += active ? F("true") : F("false");
        json += '}';
    }

    file.close();  // NOCHECK: accepted, guarded by lockSD
    SDController::unlockSD();

    json += ']';
    sendJson(request, json);
}

// ── Validation helpers ────────────────────────────────────────────

static bool isValidUint(const String &val) {
    if (val.isEmpty()) return false;
    for (unsigned int i = 0; i < val.length(); i++) {
        if (!isDigit(val[i])) return false;
    }
    return true;
}

static bool isValidInt(const String &val) {
    if (val.isEmpty()) return false;
    unsigned int start = 0;
    if (val[0] == '-') start = 1;
    if (start >= val.length()) return false;
    for (unsigned int i = start; i < val.length(); i++) {
        if (!isDigit(val[i])) return false;
    }
    return true;
}

static bool isValidFloat(const String &val) {
    if (val.isEmpty()) return false;
    char *endPtr = nullptr;
    float f = strtof(val.c_str(), &endPtr);
    if (endPtr == val.c_str()) return false;
    if (isnan(f) || isinf(f)) return false;
    return true;
}

static bool isValidBool(const String &val) {
    return val == "0" || val == "1";
}

// Validate a single entry. Returns empty string on success, error message on failure.
static String validateEntry(const String &key, const String &type, const String &value) {
    if (type == "u") {
        if (!isValidUint(value)) return key + ": must be a non-negative integer";
    } else if (type == "f") {
        if (!isValidFloat(value)) return key + ": must be a valid number";
    } else if (type == "i") {
        if (!isValidInt(value)) return key + ": must be an integer";
    } else if (type == "b") {
        if (!isValidBool(value)) return key + ": must be 0 or 1";
    } else if (type == "s") {
        if (value.isEmpty()) return key + ": must not be empty";
    }
    return "";
}

// ── POST /api/admin/globals ───────────────────────────────────────
// Receives JSON array of entries, validates, writes globals.csv to SD.
static void routePostGlobals(AsyncWebServerRequest *request, JsonVariant &json) {
    if (!checkPin(request)) {
        request->send(403);
        return;
    }
    if (!AlertState::isSdOk()) {
        sendError(request, 503, F("SD not ready"));
        return;
    }
    if (AlertState::isSdBusy()) {
        sendError(request, 409, F("SD busy"));
        return;
    }

    JsonArray arr = json.as<JsonArray>();
    if (arr.isNull() || arr.size() == 0) {
        sendError(request, 400, F("Expected non-empty JSON array"));
        return;
    }

    // First pass: validate all entries
    for (JsonVariant item : arr) {
        JsonObjectConst obj = item.as<JsonObjectConst>();
        if (obj.isNull()) continue;

        // Section markers — no validation needed
        if (obj.containsKey("section")) continue;

        if (!obj.containsKey("key") || !obj.containsKey("type") || !obj.containsKey("value")) {
            sendError(request, 400, F("Entry missing key, type, or value"));
            return;
        }

        String key   = obj["key"].as<String>();
        String type  = obj["type"].as<String>();
        String value = obj["value"].as<String>();
        bool active  = obj["active"] | true;

        // Only validate active entries (commented ones can have stale values)
        if (active) {
            String err = validateEntry(key, type, value);
            if (!err.isEmpty()) {
                String errJson = F("{\"error\":\"");
                appendJsonEscaped(errJson, err.c_str());
                errJson += F("\"}");
                request->send(400, "application/json", errJson);
                return;
            }
        }
    }

    // Second pass: write to SD
    SDController::lockSD();
    File file = SD.open("/globals.csv", FILE_WRITE);  // NOCHECK: accepted, guarded by lockSD
    if (!file) {
        SDController::unlockSD();
        sendError(request, 500, F("Cannot open globals.csv for writing"));
        return;
    }

    // Write file header
    file.println("# globals.csv - Runtime-overridable parameters");  // NOCHECK: accepted, guarded by lockSD
    file.println("# Format: key;type;value;comment");  // NOCHECK: accepted, guarded by lockSD
    file.println("# Types: u=uint, f=float, b=bool, s=string, i=int");  // NOCHECK: accepted, guarded by lockSD
    file.println("# Lines starting with # or // are ignored");  // NOCHECK: accepted, guarded by lockSD
    file.println("# Missing/corrupt file = use code defaults");  // NOCHECK: accepted, guarded by lockSD
    file.println();  // NOCHECK: accepted, guarded by lockSD

    for (JsonVariant item : arr) {
        JsonObjectConst obj = item.as<JsonObjectConst>();
        if (obj.isNull()) continue;

        // Section marker → write as section header line
        if (obj.containsKey("section")) {
            String section = obj["section"].as<String>();
            file.println();  // NOCHECK: accepted, guarded by lockSD
            String header = "# \xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90";
            file.println(header.c_str());  // NOCHECK: accepted, guarded by lockSD
            String sectionLine = "# " + section;
            file.println(sectionLine.c_str());  // NOCHECK: accepted, guarded by lockSD
            file.println(header.c_str());  // NOCHECK: accepted, guarded by lockSD
            continue;
        }

        if (!obj.containsKey("key")) continue;

        String key     = obj["key"].as<String>();
        String type    = obj["type"].as<String>();
        String value   = obj["value"].as<String>();
        String comment = obj.containsKey("comment") ? obj["comment"].as<String>() : "";
        bool active    = obj["active"] | true;

        String line;
        if (!active) line += '#';
        line += key + ';' + type + ';' + value;
        if (!comment.isEmpty()) line += ';' + comment;

        file.println(line.c_str());  // NOCHECK: accepted, guarded by lockSD
    }

    file.close();  // NOCHECK: accepted, guarded by lockSD
    SDController::unlockSD();

    sendJson(request, F("{\"status\":\"saved\"}"));
}

// ── Route registration ────────────────────────────────────────────
void attachRoutes(AsyncWebServer &server) {
    server.on("/api/admin/globals", HTTP_GET, routeGetGlobals);

    auto *postHandler = new AsyncCallbackJsonWebHandler("/api/admin/globals", routePostGlobals, 16384);
    server.addHandler(postHandler);
}

} // namespace AdminRoutes
