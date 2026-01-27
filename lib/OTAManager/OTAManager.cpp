/**
 * @file OTAManager.cpp
 * @brief Over-the-air firmware update handling
 * @version 251231E
 * @date 2025-12-31
 *
 * This implementation manages OTA firmware updates using ArduinoOTA. It provides
 * a safe two-phase update mechanism: first arm the OTA window via web interface,
 * then confirm to actually enter OTA mode. State is persisted in NVS to survive
 * reboots. When in OTA mode, the device connects to WiFi, initializes ArduinoOTA
 * with password protection, and waits for an upload. The OTA loop runs standalone
 * with explicit millis() timing to avoid scheduler conflicts during the critical
 * update window. Progress feedback is provided via serial output.
 */

#include <Arduino.h>
#include <Preferences.h>
#include <ArduinoOTA.h>

// -----------------------------------------------------------------------------
// Why the hand-rolled loop instead of TimerManager?
// OTA runs as a blocking, single-purpose routine where we must keep handling
// ArduinoOTA events until either the upload completes, fails, or times out.
// Keeping it self-contained with explicit millis() checks avoids complicated
// scheduler interactions during this critical window.
// -----------------------------------------------------------------------------

#include "Globals.h"         // gives HWconfig (SSID/PASS/OTA_* settings)
#include "WiFiManager.h"     // bootWiFiConnect(), isWiFiConnected()
#include "TimerManager.h"    // keep WiFi state machine ticking during OTA
#include "OTAManager.h"

static Preferences prefs; // NVS ns="ota"

static uint8_t  nvs_get8 (const char* k){ prefs.begin("ota", true);  uint8_t  v=prefs.getUChar(k,0); prefs.end(); return v; }
static void     nvs_set8 (const char* k,uint8_t v){ prefs.begin("ota", false); prefs.putUChar(k,v); prefs.end(); }
static uint32_t nvs_get32(const char* k){ prefs.begin("ota", true); uint32_t v=prefs.getUInt(k,0); prefs.end(); return v; }
static void     nvs_set32(const char* k,uint32_t v){ prefs.begin("ota", false); prefs.putUInt(k,v); prefs.end(); }

static void runOtaSetup(){
  PL("[OTA] start");
  bootWiFiConnect();
  const uint32_t connectDeadline = millis() + 60UL * 1000UL;
  while (!isWiFiConnected() && millis() < connectDeadline) {
    TimerManager::instance().update();
    delay(10);
  }
  if (!isWiFiConnected()) {
    PL("[OTA] no WiFi");
    return;
  }

  bool updateCompleted = false;
  bool updateFailed = false;

  ArduinoOTA.setPort(3232);
  ArduinoOTA.setPassword(OTA_PASSWORD);
  ArduinoOTA.setRebootOnSuccess(false);

  ArduinoOTA.onStart([&]() {
    PL("[OTA] push start");
  });

  ArduinoOTA.onProgress([&](unsigned int progress, unsigned int total) {
    if (!total) return;
    const unsigned int pct = (progress * 100U) / total;
    PF("[OTA] progress %u%%\\n", pct);
  });

  ArduinoOTA.onEnd([&]() {
    PL("[OTA] push end");
    updateCompleted = true;
  });

  ArduinoOTA.onError([&](ota_error_t error) {
    PF("[OTA] error %u\\n", static_cast<unsigned>(error));
    updateFailed = true;
  });

  ArduinoOTA.begin();
  PL("[OTA] listening on ArduinoOTA");

  const uint32_t windowMs = 5UL * 60UL * 1000UL;
  const uint32_t start = millis();
  while (!updateCompleted && !updateFailed && (millis() - start) < windowMs) {
    TimerManager::instance().update();
    ArduinoOTA.handle();
    delay(10);
  }

  if (updateCompleted) {
    PL("[OTA] OK, reboot");
    delay(100);
    ESP.restart();
  } else if (updateFailed) {
    PL("[OTA] failed");
  } else {
    PL("[OTA] timeout");
  }
}

cb_type restartDeviceCallback(){
  PL("[OTA] rebooting now");
  ESP.restart();
}

void otaArm(uint32_t window_s){
  const uint32_t nowS = millis()/1000;
  nvs_set32("enabled_until", nowS + window_s);
  nvs_set8("mode", 1);   // 0=normal,1=pending,2=ota
  PL("[OTA] enabled");
}

bool otaConfirmAndReboot(){
  const uint8_t mode = nvs_get8("mode");
  const uint32_t nowS = millis()/1000;
  const uint32_t arm  = nvs_get32("enabled_until");
  if (mode == 1 && nowS <= arm){
    nvs_set8("mode", 2);
    if (!TimerManager::instance().create(15000UL, 1, restartDeviceCallback)){
      PL("[OTA] confirm -> immediate reboot (timer fail)");
      delay(50);
      ESP.restart();
      return true;
    }
    PL("[OTA] confirm -> reboot in 15s");
    return true;
  }
  nvs_set8("mode", 0);
  PL("[OTA] confirm expired");
  return false;
}

void otaBootHandler(){
  const uint8_t mode = nvs_get8("mode");

  if (mode == 2){
    nvs_set8("mode", 0);        // clear first to avoid loops
    runOtaSetup();
    ESP.restart();
  }

  if (mode == 1){
    const uint32_t nowS = millis()/1000;
    const uint32_t arm  = nvs_get32("enabled_until");
    if (nowS > arm){
      nvs_set8("mode", 0);
      PL("[OTA] window expired");
    }
  }
}
/*
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1.0"/>
  <title>MyKwal Webinterface</title>
  <style>
    body { font-family: sans-serif; background:#f2f2f2; color:#333; padding:2em; max-width:500px; margin:auto; }
    h1 { text-align:center; font-size:1.5em; }
    .row { margin-bottom:1.5em; }
    label { display:block; margin-bottom:0.4em; font-weight:bold; }
    input[type="range"] { width:100%; }
    .value { font-size:0.9em; color:#666; text-align:right; }
    button { padding:0.6em 1em; margin-right:0.5em; }
    #status { margin-top:1em; font-size:0.9em; color:#444; white-space:pre-line; }
  </style>
</head>
<body>
  <h1>MyKwal Interface</h1>

  <div class="row">
    <label for="brightnessSlider">Brightness</label>
    <input type="range" id="brightnessSlider" min="0" max="255"/>
    <div class="value" id="brightnessValue">...</div>
  </div>

  <div class="row">
    <label for="audioSlider">Audio Volume</label>
    <input type="range" id="audioSlider" min="0" max="100"/>
    <div class="value" id="audioValue">...</div>
  </div>

  <hr/>

  <div class="row">
    <label>OTA</label>
    <button id="armBtn" type="button">Get ready for OTA (5 min)</button>
    <button id="confirmBtn" type="button">Confirm OTA</button>
    <div id="status"></div>
  </div>

  <script>
    const brightnessSlider = document.getElementById("brightnessSlider");
    const brightnessValue  = document.getElementById("brightnessValue");
    const audioSlider      = document.getElementById("audioSlider");
    const audioValue       = document.getElementById("audioValue");
    const armBtn           = document.getElementById("armBtn");
    const confirmBtn       = document.getElementById("confirmBtn");
    const statusDiv        = document.getElementById("status");

    function setStatus(txt){ statusDiv.textContent = txt; }

    // Brightness
    brightnessSlider.addEventListener("input", () => {
      fetch(`/setBrightness?value=${brightnessSlider.value}`);
      brightnessValue.textContent = brightnessSlider.value;
    });

    // Audio
    audioSlider.addEventListener("input", () => {
      const v = audioSlider.value / 100.0;
      fetch(`/setWebAudioLevel?value=${v.toFixed(2)}`);
      audioValue.textContent = v.toFixed(2);
    });

    // Init from device
    fetch("/getBrightness")
      .then(r => r.text())
      .then(v => { brightnessSlider.value = v; brightnessValue.textContent = v; })
      .catch(()=>{});

    fetch("/getWebAudioLevel")
      .then(r => r.text())
      .then(t => { const v = parseFloat(t)||0; audioSlider.value = Math.round(v*100); audioValue.textContent = v.toFixed(2); })
      .catch(()=>{});

    // OTA actions
    armBtn.addEventListener("click", () => {
      setStatus("Arming OTA window…");
      fetch("/ota/arm")
        .then(r => r.text())
        .then(t => setStatus(`OTA arm: ${t}`))
        .catch(e => setStatus(`OTA arm error`));
    });

    confirmBtn.addEventListener("click", () => {
      setStatus("Confirming OTA… device will reboot if accepted.");
      fetch("/ota/confirm", { method: "POST" })
        .then(async r => {
          const t = await r.text();
          if (r.ok) setStatus(t); else setStatus(`Error: ${t}`);
        })
        .catch(() => setStatus("OTA confirm error"));
    });
  </script>
</body>
</html>

*/
