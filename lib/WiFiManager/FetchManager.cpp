/**
 * @file FetchManager.cpp
 * @brief HTTP fetch for weather/sunrise APIs and NTP time
 * @version 251231H
 * @date 2025-12-31
 *
 * Implementation of external data fetching services.
 * Handles NTP time synchronization with configurable servers
 * and timezone support. Fetches weather and sunrise/sunset
 * data from external APIs. Uses TimerManager for scheduled
 * updates and retry logic with non-blocking HTTP operations.
 */

#include "FetchManager.h"
#include "Globals.h"
#include "TimerManager.h"
#include "PRTClock.h"
#include "ContextManager.h"
#include "SDManager.h"
#include "RunManager.h"
#include "AudioState.h"
#include "Alert/AlertState.h"

#include <WiFiUdp.h>
#include <NTPClient.h>
#include <Timezone.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <sys/time.h>

// --- Config ---
// HTTPS versions - often fail on ESP32 due to TLS handshake issues (code -1)
// #define SUN_URL     "https://api.sunrise-sunset.org/json?lat=52.3702&lng=4.8952&formatted=0"
// #define WEATHER_URL "https://api.open-meteo.com/v1/forecast?latitude=52.37&longitude=4.89&daily=temperature_2m_max,temperature_2m_min&timezone=auto"

// HTTP versions - more reliable on ESP32
#define SUN_URL     "http://api.sunrise-sunset.org/json?lat=52.3702&lng=4.8952&formatted=0"
#define WEATHER_URL "http://api.open-meteo.com/v1/forecast?latitude=52.37&longitude=4.89&daily=temperature_2m_max,temperature_2m_min&timezone=auto"

#define DEBUG_FETCH 1

// --- Timezone for Europe/Amsterdam ---
TimeChangeRule CEST = {"CEST", Last, Sun, Mar, 2, 120};
TimeChangeRule CET  = {"CET", Last, Sun, Oct, 3, 60};
Timezone CE(CEST, CET);

// --- NTP Client ---
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0, 60000);

// --- Forward declarations ---
static void cb_fetchNTP();
static void cb_fetchWeather();
static void cb_fetchSunrise();
static bool fetchUrlToString(const char *url, String &output);
static void saveTimeToSD(const struct tm &t);
static bool loadTimeFromSD(PRTClock &clock);



// ===================================================
// NTP / Time fetch
// ===================================================
static void cb_fetchNTP() {
    static bool clientStarted = false;
    static bool wifiWarned = false;

    if (prtClock.isTimeFetched()) {
        return;
    }

    // Update boot status with remaining retries
    int remaining = timers.getRepeatCount(cb_fetchNTP);
    if (remaining != -1)
        AlertState::set(SC_NTP, abs(remaining));

    // Check if last retry (abs(remaining) == 1 means final attempt)
    bool lastRetry = (abs(remaining) == 1);
    
    // Policy: defer fetch if audio is playing
    if (isSentencePlaying()) {
        return;  // Skip this attempt, timer continues
    }

    if (!AlertState::isWifiOk()) {
        if (DEBUG_FETCH && !wifiWarned) {
            PL("[Fetch] No WiFi, waiting before NTP");
            wifiWarned = true;
        }
        if (lastRetry) {
            AlertState::setNtpStatus(false);
            PL("[Fetch] NTP gave up after retries (no WiFi)");
        }
        return;
    }

    if (!clientStarted) {
        timeClient.begin();
        clientStarted = true;
    }

    if (DEBUG_FETCH) {
        PL("[Fetch] Trying NTP/time fetch...");
    }

    if (!timeClient.update()) {
        if (lastRetry) {
            AlertState::setNtpStatus(false);
            PL("[Fetch] NTP gave up after retries");
        } else if (DEBUG_FETCH) {
            PL("[Fetch] NTP update failed, will retry");
        }
        return;
    }

    wifiWarned = false;

    time_t utc   = timeClient.getEpochTime();
    time_t local = CE.toLocal(utc);

    // Set ESP32 system time so SD library uses correct timestamps
    struct timeval tv = { .tv_sec = local, .tv_usec = 0 };
    settimeofday(&tv, nullptr);

    struct tm t;
    localtime_r(&local, &t);

    prtClock.setHour(t.tm_hour);
    prtClock.setMinute(t.tm_min);
    prtClock.setSecond(t.tm_sec);
    prtClock.setYear(t.tm_year + 1900 - 2000);
    prtClock.setMonth(t.tm_mon + 1);
    prtClock.setDay(t.tm_mday);
    prtClock.setDoW(t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
    prtClock.setDoY(t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);

    if (DEBUG_FETCH) {
        PF("[Fetch] Time update: %02d:%02d:%02d (%d-%02d-%02d)\n",
           t.tm_hour, t.tm_min, t.tm_sec,
           t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
    }

    prtClock.setTimeFetched(true);
    AlertState::setNtpStatus(true);
    timers.cancel(cb_fetchNTP);
    prtClock.setMoonPhaseValue();
    RunManager::requestSyncRtcFromClock();
}

// ===================================================
// Weather fetch
// ===================================================

static void cb_fetchWeather() {
    // Update boot status with remaining retries
    int remaining = timers.getRepeatCount(cb_fetchWeather);
    if (remaining != -1)
        AlertState::set(SC_WEATHER, abs(remaining));

    // Check if last retry (abs(remaining) == 1 means final attempt)
    bool lastRetry = (abs(remaining) == 1);

    // Policy: defer fetch if audio is playing
    if (isSentencePlaying()) {
        return;  // Skip this attempt, timer continues
    }
    
    if (!AlertState::isWifiOk()) {
        if (DEBUG_FETCH) {
            PL("[Fetch] No WiFi, skipping weather");
        }
        ContextManager::clearWeather();
        if (lastRetry) {
            AlertState::setWeatherStatus(false);
            PL("[Fetch] Weather gave up after retries (no WiFi)");
        }
        return;
    }
    if (!prtClock.isTimeFetched()) {
        if (DEBUG_FETCH) {
            PL("[Fetch] No NTP/time, skipping weather");
        }
        ContextManager::clearWeather();
        if (lastRetry) {
            AlertState::setWeatherStatus(false);
            PL("[Fetch] Weather gave up after retries (no time)");
        }
        return;
    }

    String response;
    if (!fetchUrlToString(WEATHER_URL, response)) {
        ContextManager::clearWeather();
        if (lastRetry) {
            AlertState::setWeatherStatus(false);
            PL("[Fetch] Weather gave up after retries");
        } else if (DEBUG_FETCH) {
            PL("[Fetch] Weather fetch failed, will retry");
        }
        return;
    }

    int idxMin = response.indexOf("\"temperature_2m_min\":[");
    int idxMax = response.indexOf("\"temperature_2m_max\":[");
    if (idxMin == -1 || idxMax == -1) {
        ContextManager::clearWeather();
        return;
    }

    int minStart = response.indexOf("[", idxMin);
    int minEnd   = response.indexOf("]", idxMin);
    int maxStart = response.indexOf("[", idxMax);
    int maxEnd   = response.indexOf("]", idxMax);
    if (minStart == -1 || minEnd == -1 || maxStart == -1 || maxEnd == -1) {
        ContextManager::clearWeather();
        return;
    }

    float tMin = response.substring(minStart + 1, minEnd).toFloat();
    float tMax = response.substring(maxStart + 1, maxEnd).toFloat();
    ContextManager::updateWeather(tMin, tMax);
    AlertState::setWeatherStatus(true);

    if (DEBUG_FETCH) {
        PF("[Fetch] Ext. Temperature: min=%.1f max=%.1f\n", tMin, tMax);
    }

    timers.cancel(cb_fetchWeather);
}

// ===================================================
// Sunrise/Sunset fetch
// ===================================================
   int extInt(String s, uint8_t n1, uint8_t n2){
        return s.substring(n1, n2).toInt();
    }
static void cb_fetchSunrise() {
    // Policy: defer fetch if audio is playing
    if (isSentencePlaying()) {
        return;  // Skip this attempt, timer continues
    }
    
    if (!AlertState::isWifiOk()) {
        if (DEBUG_FETCH) {
            PL("[Fetch] No WiFi, skipping sun data");
        }
        timers.restart(Globals::sunRefreshIntervalMs, 0, cb_fetchSunrise);
        return;
    }
    if (!prtClock.isTimeFetched()) {
        if (DEBUG_FETCH) {
            PL("[Fetch] No NTP/time, skipping sun data");
        }
        timers.restart(Globals::sunRefreshIntervalMs, 0, cb_fetchSunrise);
        return;
    }

    String response;
    if (!fetchUrlToString(SUN_URL, response)) {
        if (DEBUG_FETCH) {
            PL("[Fetch] Sun fetch failed, will retry");
        }
        timers.restart(Globals::sunRefreshIntervalMs, 0, cb_fetchSunrise);
        return;
    }

    int sr = response.indexOf("\"sunrise\":\"");
    int ss = response.indexOf("\"sunset\":\"");
    if (sr == -1 || ss == -1) return;

    String sunriseIso = response.substring(sr + 11, sr + 30);
    String sunsetIso  = response.substring(ss + 10, ss + 29);

    struct tm tmRise = {};
    struct tm tmSet  = {};

    tmRise.tm_year = extInt(sunriseIso, 0, 4) - 1900;
    tmRise.tm_mon  = extInt(sunriseIso, 5, 7) - 1;
    tmRise.tm_mday = extInt(sunriseIso, 8, 10);
    tmRise.tm_hour = extInt(sunriseIso, 11, 13);
    tmRise.tm_min  = extInt(sunriseIso, 14, 16);
    tmRise.tm_sec  = extInt(sunriseIso, 17, 19);

    tmSet.tm_year = extInt(sunsetIso, 0, 4) - 1900;
    tmSet.tm_mon  = extInt(sunsetIso, 5, 7) - 1;
    tmSet.tm_mday = extInt(sunsetIso, 8, 10);
    tmSet.tm_hour = extInt(sunsetIso, 11, 13);
    tmSet.tm_min  = extInt(sunsetIso, 14, 16);
    tmSet.tm_sec  = extInt(sunsetIso, 17, 19);

    struct tm ltmRise, ltmSet;
    time_t localRise = CE.toLocal(mktime(&tmRise));
    time_t localSet  = CE.toLocal(mktime(&tmSet));
    localtime_r(&localRise, &ltmRise);
    localtime_r(&localSet, &ltmSet);

    prtClock.setSunriseHour(ltmRise.tm_hour);
    prtClock.setSunriseMinute(ltmRise.tm_min);
    prtClock.setSunsetHour(ltmSet.tm_hour);
    prtClock.setSunsetMinute(ltmSet.tm_min);

    if (DEBUG_FETCH) {
        PF("[Fetch] Sunrise/Sunset (local): up %02d:%02d, down %02d:%02d\n",
           ltmRise.tm_hour, ltmRise.tm_min, ltmSet.tm_hour, ltmSet.tm_min);
    }

    timers.cancel(cb_fetchSunrise);
}

// ===================================================
// Init entry point
// ===================================================
bool bootFetchManager() {
    if (!AlertState::isWifiOk()) {
        if (DEBUG_FETCH) PL("[Fetch] WiFi not ready, fetchers not scheduled");
        return false;
    }

    timers.cancel(cb_fetchNTP);
    timers.cancel(cb_fetchWeather);
    timers.cancel(cb_fetchSunrise);

    prtClock.setTimeFetched(false);
    bool ntpOk = timers.create(1000, 25, cb_fetchNTP, 1.5f);
    bool weatherOk = timers.create(2000, 24, cb_fetchWeather, 1.5f);
    bool sunOk = timers.create(3000, 0, cb_fetchSunrise);

    if (!ntpOk && DEBUG_FETCH) PL("[Fetch] Failed to schedule NTP timer");
    if (!weatherOk && DEBUG_FETCH) PL("[Fetch] Failed to schedule weather timer");
    if (!sunOk && DEBUG_FETCH) PL("[Fetch] Failed to schedule sun timer");

    return ntpOk && weatherOk && sunOk;
}

// ===================================================
// Public API: request NTP re-sync (called at midnight)
// ===================================================
namespace FetchManager {
    void requestNtpResync() {
        PL("[Fetch] Midnight NTP resync requested");
        timers.cancel(cb_fetchNTP);
        timers.create(100, 34, cb_fetchNTP, 1.5f);
    }
}

// ===================================================
// Low-level HTTP fetch
// ===================================================
static bool fetchUrlToString(const char *url, String &output) {
    if (!AlertState::isWifiOk()) return false;

    HTTPClient http;
    http.setTimeout(10000);  // 10 second timeout
    
    // Use plain WiFiClient for HTTP, WiFiClientSecure for HTTPS
    if (strncmp(url, "https://", 8) == 0) {
        WiFiClientSecure client;
        client.setInsecure();  // Skip certificate verification
        http.begin(client, url);
    } else {
        WiFiClient client;
        http.begin(client, url);
    }
    
    int httpCode = http.GET();
    
    if (httpCode != 200) {
        if (DEBUG_FETCH) {
            PF("[Fetch] HTTP GET failed: code %d\n", httpCode);
        }
        http.end();
        return false;  // Policy reschedules refetch
    }
    
    output = http.getString();
    http.end();
    return (output.length() > 0);
}
