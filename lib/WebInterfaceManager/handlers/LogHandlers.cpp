#include <Arduino.h>
#include "LogHandlers.h"
#include "LogBuffer.h"

namespace LogHandlers {

void handleLog(AsyncWebServerRequest *request) {
    size_t len = LogBuffer::available();
    char* temp = new char[len + 1];
    LogBuffer::read(temp, len);
    temp[len] = '\0';
    request->send(200, "text/plain", temp);
    delete[] temp;
}

void handleLogClear(AsyncWebServerRequest *request) {
    LogBuffer::clear();
    request->send(200, "text/plain", "OK");
}

void attachRoutes(AsyncWebServer &server) {
    // Register more specific route first to avoid prefix matching
    server.on("/log/clear", HTTP_GET, handleLogClear);
    server.on("/log", HTTP_GET, handleLog);
}

} // namespace LogHandlers
