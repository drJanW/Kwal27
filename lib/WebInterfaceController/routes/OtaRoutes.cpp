/**
 * @file OtaRoutes.cpp
 * @brief OTA update API endpoint — HTTP firmware upload
 * @version 260212A
 * @date 2026-02-12
 */
#include <Arduino.h>
#include "OtaRoutes.h"
#include "Globals.h"
#include "TimerManager.h"
#include <Update.h>

namespace OtaRoutes {

// ── HTTP OTA: firmware upload via browser POST ──────────────────

namespace {
    bool     otaUploadStarted = false;
    bool     otaUploadFailed  = false;
    size_t   otaUploadTotal   = 0;
    String   otaUploadError;

    void cb_rebootAfterOta() {
        ESP.restart();
    }
}

/// Multipart data handler: receives firmware chunks
void routeUploadFirmwareData(AsyncWebServerRequest *request,
                             const String & /*filename*/,
                             size_t index, uint8_t *data, size_t len, bool final)
{
    if (index == 0) {
        otaUploadStarted = true;
        otaUploadFailed  = false;
        otaUploadTotal   = 0;
        otaUploadError   = "";

        size_t contentLen = request->contentLength();
        PF("[OTA-HTTP] Begin upload, content-length=%u\n", contentLen);

        if (!Update.begin(contentLen != 0 ? contentLen : UPDATE_SIZE_UNKNOWN)) {
            otaUploadFailed = true;
            otaUploadError  = Update.errorString();
            PF("[OTA-HTTP] Update.begin failed: %s\n", otaUploadError.c_str());
            return;
        }
    }

    if (otaUploadFailed) return;

    if (len > 0) {
        if (Update.write(data, len) != len) {
            otaUploadFailed = true;
            otaUploadError  = Update.errorString();
            PF("[OTA-HTTP] Update.write failed: %s\n", otaUploadError.c_str());
            return;
        }
        otaUploadTotal += len;
    }

    if (final) {
        if (Update.end(true)) {
            PF("[OTA-HTTP] Upload complete: %u bytes\n", otaUploadTotal);
        } else {
            otaUploadFailed = true;
            otaUploadError  = Update.errorString();
            PF("[OTA-HTTP] Update.end failed: %s\n", otaUploadError.c_str());
        }
    }
}

/// Request handler: called after all multipart data is received
void routeUploadFirmwareRequest(AsyncWebServerRequest *request)
{
    if (otaUploadFailed || !Update.isFinished()) {
        String msg = F("{\"status\":\"error\",\"error\":\"");
        msg += otaUploadError.length() ? otaUploadError : F("Upload failed");
        msg += F("\"}");
        request->send(400, "application/json", msg);
        otaUploadStarted = false;
        return;
    }

    String msg = F("{\"status\":\"ok\",\"size\":");
    msg += otaUploadTotal;
    msg += F("}");

    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", msg);
    response->addHeader("Connection", "close");
    request->send(response);

    // Reboot after 2 seconds so response reaches browser
    timers.create(2000, 1, cb_rebootAfterOta);
    PL("[OTA-HTTP] Reboot scheduled in 2s");
}

void attachRoutes(AsyncWebServer &server)
{
    server.on("/ota/upload", HTTP_POST, routeUploadFirmwareRequest, routeUploadFirmwareData);
}

} // namespace OtaRoutes