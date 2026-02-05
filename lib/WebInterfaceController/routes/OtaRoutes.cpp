/**
 * @file OtaRoutes.cpp
 * @brief OTA update API endpoint routes
 * @version 260205A
 * @date 2026-02-05
 */
#include "OtaRoutes.h"
#include "Globals.h"
#include "RunManager.h"

namespace OtaRoutes {

/// Route handler: arm OTA update window (300s)
void routeArm(AsyncWebServerRequest *request)
{
    RunManager::requestArmOTA(300);
    request->send(200, "text/plain", "OK");
}

/// Route handler: confirm OTA and prepare for reboot
void routeConfirm(AsyncWebServerRequest *request)
{
    if (RunManager::requestConfirmOTA()) {
        AsyncWebServerResponse *response = request->beginResponse(
            200, "text/plain", "Reboot within 15s - press Enter in ota.bat");
        response->addHeader("Connection", "close");
        request->send(response);
    } else {
        request->send(400, "text/plain", "EXPIRED");
    }
}

/// Route handler: combined arm + confirm (single click start)
void routeStart(AsyncWebServerRequest *request)
{
    RunManager::requestArmOTA(300);
    if (RunManager::requestConfirmOTA()) {
        AsyncWebServerResponse *response = request->beginResponse(
            200, "text/plain", "Reboot within 15s - press Enter in ota.bat");
        response->addHeader("Connection", "close");
        request->send(response);
    } else {
        request->send(500, "text/plain", "OTA start failed");
    }
}

void attachRoutes(AsyncWebServer &server)
{
    server.on("/ota/arm", HTTP_GET, routeArm);
    server.on("/ota/confirm", HTTP_POST, routeConfirm);
    server.on("/ota/start", HTTP_POST, routeStart);
}

} // namespace OtaRoutes