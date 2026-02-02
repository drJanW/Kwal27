/**
 * @file OtaRoutes.cpp
 * @brief OTA update API endpoint routes
 * @version 251231E
 * @date 2025-12-31
 *
 * Implements HTTP routes for the /ota/* endpoints.
 * Provides a two-step OTA update flow: arm (prepare for update)
 * and confirm (trigger reboot for update). Also supports a combined
 * start endpoint. Integrates with RunManager for OTA state control.
 */

#include "OtaRoutes.h"
#include "Globals.h"
#include "RunManager.h"

namespace OtaRoutes {

void routeArm(AsyncWebServerRequest *request)
{
    RunManager::requestArmOTA(300);
    request->send(200, "text/plain", "OK");
}

void routeConfirm(AsyncWebServerRequest *request)
{
    if (RunManager::requestConfirmOTA()) {
        AsyncWebServerResponse *response = request->beginResponse(
            200, "text/plain", "Reboot binnen 15s - druk Enter in ota.bat");
        response->addHeader("Connection", "close");
        request->send(response);
    } else {
        request->send(400, "text/plain", "EXPIRED");
    }
}

void routeStart(AsyncWebServerRequest *request)
{
    RunManager::requestArmOTA(300);
    if (RunManager::requestConfirmOTA()) {
        AsyncWebServerResponse *response = request->beginResponse(
            200, "text/plain", "Reboot binnen 15s - druk Enter in ota.bat");
        response->addHeader("Connection", "close");
        request->send(response);
    } else {
        request->send(500, "text/plain", "OTA start mislukt");
    }
}

void attachRoutes(AsyncWebServer &server)
{
    server.on("/ota/arm", HTTP_GET, routeArm);
    server.on("/ota/confirm", HTTP_POST, routeConfirm);
    server.on("/ota/start", HTTP_POST, routeStart);
}

} // namespace OtaRoutes