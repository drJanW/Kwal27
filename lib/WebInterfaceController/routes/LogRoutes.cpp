/**
 * @file LogRoutes.cpp
 * @brief LogRoutes implementation
 * @version 260202A
 $12026-02-05
 */
#include <Arduino.h>
#include "LogRoutes.h"
#include "LogBuffer.h"

namespace LogRoutes {

void routeLog(AsyncWebServerRequest *request) {
    size_t len = LogBuffer::available();
    char* temp = new char[len + 1];
    LogBuffer::read(temp, len);
    temp[len] = '\0';
    request->send(200, "text/plain", temp);
    delete[] temp;
}

void routeLogClear(AsyncWebServerRequest *request) {
    LogBuffer::clear();
    request->send(200, "text/plain", "OK");
}

void attachRoutes(AsyncWebServer &server) {
    // Register more specific route first to avoid prefix matching
    server.on("/log/clear", HTTP_GET, routeLogClear);
    server.on("/log", HTTP_GET, routeLog);
}

} // namespace LogRoutes