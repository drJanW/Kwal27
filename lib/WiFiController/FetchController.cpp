/**
 * @file FetchController.cpp
 * @brief HTTP fetch for weather/sunrise APIs and NTP time
 * @version 260214A
 * @date 2026-02-14
 */
#include <Arduino.h>
#include "FetchController.h"
#include "Globals.h"
#include "TimerManager.h"
#include "PRTClock.h"
#include "ContextController.h"
#include "SDController.h"
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
// URLs built dynamically from Globals::locationLat / locationLon
static char sunUrl[128];
static char weatherUrl[192];

static void buildLocationUrls() {
    snprintf(sunUrl, sizeof(sunUrl),
        "http://api.sunrise-sunset.org/json?lat=%.4f&lng=%.4f&formatted=0",
        Globals::locationLat, Globals::locationLon);
    snprintf(weatherUrl, sizeof(weatherUrl),
        "http://api.open-meteo.com/v1/forecast?latitude=%.2f&longitude=%.2f"
        "&daily=temperature_2m_max,temperature_2m_min&timezone=auto",
        Globals::locationLat, Globals::locationLon);
}

#define DEBUG_FETCH LOG_BOOT_SPAM

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
    auto remaining = timers.remaining();
    AlertState::set(SC_NTP, remaining);

    // Check if last retry (remaining == 1 means final attempt)
    bool lastRetry = (remaining == 1);
    
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
static bool weatherFetched = false;

static void cb_fetchWeather() {
    // Update boot status with remaining retries
    auto remaining = timers.remaining();
    AlertState::set(SC_WEATHER, remaining);

    // Check if last retry (remaining == 1 means final attempt)
    bool lastRetry = (remaining == 1);

    // Policy: defer fetch if audio is playing
    if (isSentencePlaying()) {
        return;  // Skip this attempt, timer continues
    }
    
    if (!AlertState::isWifiOk()) {
        if (DEBUG_FETCH) {
            PL("[Fetch] No WiFi, skipping weather");
        }
        if (!weatherFetched) ContextController::clearWeather();
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
        if (!weatherFetched) ContextController::clearWeather();
        if (lastRetry) {
            AlertState::setWeatherStatus(false);
            PL("[Fetch] Weather gave up after retries (no time)");
        }
        return;
    }

    String response;
    if (!fetchUrlToString(weatherUrl, response)) {
        if (!weatherFetched) ContextController::clearWeather();
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
        if (!weatherFetched) ContextController::clearWeather();
        return;
    }

    int minStart = response.indexOf("[", idxMin);
    int minEnd   = response.indexOf("]", idxMin);
    int maxStart = response.indexOf("[", idxMax);
    int maxEnd   = response.indexOf("]", idxMax);
    if (minStart == -1 || minEnd == -1 || maxStart == -1 || maxEnd == -1) {
        if (!weatherFetched) ContextController::clearWeather();
        return;
    }

    float tMin = response.substring(minStart + 1, minEnd).toFloat();
    float tMax = response.substring(maxStart + 1, maxEnd).toFloat();

    ContextController::updateWeather(tMin, tMax);
    AlertState::setWeatherStatus(true);

    if (!weatherFetched) {
        weatherFetched = true;
        // Switch from boot retries to periodic refresh
        timers.cancel(cb_fetchWeather);
        timers.create(Globals::weatherRefreshIntervalMs, 0, cb_fetchWeather);
    } else if (DEBUG_FETCH) {
        PF("[Fetch] Weather updated: min=%.1f max=%.1f\n", tMin, tMax);
    }
}

// ===================================================
// Sunrise / Sunset fetch
// ===================================================

static void cb_fetchSunrise() {
    // Update boot status with remaining retries
    auto remaining = timers.remaining();
    (void)remaining;

    // Policy: defer fetch if audio is playing
    if (isSentencePlaying()) {
        return;  // Skip this attempt, timer continues
    }

    if (!AlertState::isWifiOk()) {
        if (DEBUG_FETCH) {
            PL("[Fetch] No WiFi, skipping sunrise");
        }
        prtClock.setSunriseHour(0);
        prtClock.setSunriseMinute(0);
        prtClock.setSunsetHour(0);
        prtClock.setSunsetMinute(0);
        ContextController::refreshTimeRead();
        return;
    }
    if (!prtClock.isTimeFetched()) {
        if (DEBUG_FETCH) {
            PL("[Fetch] No NTP/time, skipping sunrise");
        }
        prtClock.setSunriseHour(0);
        prtClock.setSunriseMinute(0);
        prtClock.setSunsetHour(0);
        prtClock.setSunsetMinute(0);
        ContextController::refreshTimeRead();
        return;
    }

    String response;
    if (!fetchUrlToString(sunUrl, response)) {
        prtClock.setSunriseHour(0);
        prtClock.setSunriseMinute(0);
        prtClock.setSunsetHour(0);
        prtClock.setSunsetMinute(0);
        ContextController::refreshTimeRead();
        if (DEBUG_FETCH) {
            PL("[Fetch] Sunrise fetch failed, will retry");
        }
        return;
    }

    int idxRise = response.indexOf("\"sunrise\":\"");
    int idxSet  = response.indexOf("\"sunset\":\"");
    if (idxRise == -1 || idxSet == -1) {
        prtClock.setSunriseHour(0);
        prtClock.setSunriseMinute(0);
        prtClock.setSunsetHour(0);
        prtClock.setSunsetMinute(0);
        ContextController::refreshTimeRead();
        return;
    }

    int riseStart = response.indexOf("\"", idxRise + 10);
    int riseEnd   = response.indexOf("\"", riseStart + 1);
    int setStart  = response.indexOf("\"", idxSet + 9);
    int setEnd    = response.indexOf("\"", setStart + 1);

    if (riseStart == -1 || riseEnd == -1 || setStart == -1 || setEnd == -1) {
        prtClock.setSunriseHour(0);
        prtClock.setSunriseMinute(0);
        prtClock.setSunsetHour(0);
        prtClock.setSunsetMinute(0);
        ContextController::refreshTimeRead();
        return;
    }

    String sunriseUtc = response.substring(riseStart + 1, riseEnd);
    String sunsetUtc  = response.substring(setStart + 1, setEnd);

    const int riseT = sunriseUtc.indexOf('T');
    const int setT = sunsetUtc.indexOf('T');
    if (riseT < 0 || setT < 0) {
        prtClock.setSunriseHour(0);
        prtClock.setSunriseMinute(0);
        prtClock.setSunsetHour(0);
        prtClock.setSunsetMinute(0);
        ContextController::refreshTimeRead();
        return;
    }

    const int riseYear = sunriseUtc.substring(0, 4).toInt();
    const int riseMonth = sunriseUtc.substring(5, 7).toInt();
    const int riseDay = sunriseUtc.substring(8, 10).toInt();
    const int riseHour = sunriseUtc.substring(riseT + 1, riseT + 3).toInt();
    const int riseMinute = sunriseUtc.substring(riseT + 4, riseT + 6).toInt();
    const int riseSecond = sunriseUtc.substring(riseT + 7, riseT + 9).toInt();

    const int setYear = sunsetUtc.substring(0, 4).toInt();
    const int setMonth = sunsetUtc.substring(5, 7).toInt();
    const int setDay = sunsetUtc.substring(8, 10).toInt();
    const int setHour = sunsetUtc.substring(setT + 1, setT + 3).toInt();
    const int setMinute = sunsetUtc.substring(setT + 4, setT + 6).toInt();
    const int setSecond = sunsetUtc.substring(setT + 7, setT + 9).toInt();

    if (riseYear < 2000 || riseMonth < 1 || riseMonth > 12 || riseDay < 1 || riseDay > 31 ||
        riseHour < 0 || riseHour > 23 || riseMinute < 0 || riseMinute > 59 || riseSecond < 0 || riseSecond > 59 ||
        setYear < 2000 || setMonth < 1 || setMonth > 12 || setDay < 1 || setDay > 31 ||
        setHour < 0 || setHour > 23 || setMinute < 0 || setMinute > 59 || setSecond < 0 || setSecond > 59) {
        prtClock.setSunriseHour(0);
        prtClock.setSunriseMinute(0);
        prtClock.setSunsetHour(0);
        prtClock.setSunsetMinute(0);
        ContextController::refreshTimeRead();
        return;
    }

    struct tm riseUtcTm{};
    riseUtcTm.tm_year = riseYear - 1900;
    riseUtcTm.tm_mon = riseMonth - 1;
    riseUtcTm.tm_mday = riseDay;
    riseUtcTm.tm_hour = riseHour;
    riseUtcTm.tm_min = riseMinute;
    riseUtcTm.tm_sec = riseSecond;

    struct tm setUtcTm{};
    setUtcTm.tm_year = setYear - 1900;
    setUtcTm.tm_mon = setMonth - 1;
    setUtcTm.tm_mday = setDay;
    setUtcTm.tm_hour = setHour;
    setUtcTm.tm_min = setMinute;
    setUtcTm.tm_sec = setSecond;

    const int riseYearAdj = riseYear - (riseMonth <= 2);
    const int riseEra = (riseYearAdj >= 0 ? riseYearAdj : riseYearAdj - 399) / 400;
    const unsigned riseYoe = static_cast<unsigned>(riseYearAdj - riseEra * 400);
    const unsigned riseDoy = (153 * (riseMonth + (riseMonth > 2 ? -3 : 9)) + 2) / 5 + riseDay - 1;
    const unsigned riseDoe = riseYoe * 365 + riseYoe / 4 - riseYoe / 100 + riseDoy;
    const int64_t riseDays = static_cast<int64_t>(riseEra) * 146097 + static_cast<int64_t>(riseDoe) - 719468;
    const int64_t riseUtcSeconds = riseDays * 86400LL +
        static_cast<int64_t>(riseHour) * 3600LL +
        static_cast<int64_t>(riseMinute) * 60LL +
        static_cast<int64_t>(riseSecond);

    const int setYearAdj = setYear - (setMonth <= 2);
    const int setEra = (setYearAdj >= 0 ? setYearAdj : setYearAdj - 399) / 400;
    const unsigned setYoe = static_cast<unsigned>(setYearAdj - setEra * 400);
    const unsigned setDoy = (153 * (setMonth + (setMonth > 2 ? -3 : 9)) + 2) / 5 + setDay - 1;
    const unsigned setDoe = setYoe * 365 + setYoe / 4 - setYoe / 100 + setDoy;
    const int64_t setDays = static_cast<int64_t>(setEra) * 146097 + static_cast<int64_t>(setDoe) - 719468;
    const int64_t setUtcSeconds = setDays * 86400LL +
        static_cast<int64_t>(setHour) * 3600LL +
        static_cast<int64_t>(setMinute) * 60LL +
        static_cast<int64_t>(setSecond);

    if (riseUtcSeconds <= 0 || setUtcSeconds <= 0) {
        prtClock.setSunriseHour(0);
        prtClock.setSunriseMinute(0);
        prtClock.setSunsetHour(0);
        prtClock.setSunsetMinute(0);
        ContextController::refreshTimeRead();
        return;
    }

    const time_t riseUtc = static_cast<time_t>(riseUtcSeconds);
    const time_t setUtc = static_cast<time_t>(setUtcSeconds);
    const time_t riseLocal = CE.toLocal(riseUtc);
    const time_t setLocal = CE.toLocal(setUtc);

    struct tm riseLocalTm{};
    struct tm setLocalTm{};
    localtime_r(&riseLocal, &riseLocalTm);
    localtime_r(&setLocal, &setLocalTm);

    prtClock.setSunriseHour(static_cast<uint8_t>(riseLocalTm.tm_hour));
    prtClock.setSunriseMinute(static_cast<uint8_t>(riseLocalTm.tm_min));
    prtClock.setSunsetHour(static_cast<uint8_t>(setLocalTm.tm_hour));
    prtClock.setSunsetMinute(static_cast<uint8_t>(setLocalTm.tm_min));
    ContextController::refreshTimeRead();

    if (DEBUG_FETCH) {
        PF("[Fetch] Sunrise updated: rise=%s set=%s\n", sunriseUtc.c_str(), sunsetUtc.c_str());
    }
}


// ===================================================
// HTTP fetch helper
// ===================================================
static bool fetchUrlToString(const char *url, String &output) {
    HTTPClient http;
    WiFiClient client;
    http.begin(client, url);
    int httpCode = http.GET();

    if (httpCode <= 0) {
        if (DEBUG_FETCH) {
            PF("[Fetch] HTTP GET failed: %s\n", http.errorToString(httpCode).c_str());
        }
        http.end();
        return false;
    }

    if (httpCode != HTTP_CODE_OK) {
        if (DEBUG_FETCH) {
            PF("[Fetch] HTTP GET failed: code %d\n", httpCode);
        }
        http.end();
        return false;
    }

    output = http.getString();
    http.end();
    return true;
}

// ===================================================
// Save/load time from SD (used if NTP fails)
// ===================================================
static void saveTimeToSD(const struct tm &t) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
             t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
             t.tm_hour, t.tm_min, t.tm_sec);
    SDController::writeTextFile("/config/last_time.txt", buf);
}

static bool loadTimeFromSD(PRTClock &clock) {
    if (!SDController::fileExists("/config/last_time.txt")) {
        return false;
    }
    String content = SDController::readTextFile("/config/last_time.txt");
    if (content.length() < 19) {
        return false;
    }

    int year  = content.substring(0, 4).toInt();
    int month = content.substring(5, 7).toInt();
    int day   = content.substring(8, 10).toInt();
    int hour  = content.substring(11, 13).toInt();
    int min   = content.substring(14, 16).toInt();
    int sec   = content.substring(17, 19).toInt();

    clock.setYear(year - 2000);
    clock.setMonth(month);
    clock.setDay(day);
    clock.setHour(hour);
    clock.setMinute(min);
    clock.setSecond(sec);
    clock.setDoW(year, month, day);
    clock.setDoY(year, month, day);

    return true;
}

// ===================================================
// Boot sequence and request API
// ===================================================

bool bootFetchController() {
    buildLocationUrls();

    if (!AlertState::isWifiOk()) {
        if (DEBUG_FETCH) {
            PL("[Fetch] boot aborted, no WiFi");
        }
        return false;
    }

    // Load time from SD if available before trying NTP
    if (loadTimeFromSD(prtClock)) {
        prtClock.setTimeFetched(true);
        AlertState::setNtpStatus(true);
        if (DEBUG_FETCH) {
            PL("[Fetch] Time loaded from SD");
        }
    }

    // NTP retry timer
    timers.create(Globals::clockBootstrapIntervalMs, Globals::wifiRetryCount, cb_fetchNTP, Globals::wifiRetryGrowth);
    // Weather: boot retries with growing intervals, switches to periodic on success
    weatherFetched = false;
    timers.create(Globals::weatherBootstrapIntervalMs, Globals::wifiRetryCount, cb_fetchWeather, Globals::wifiRetryGrowth);
    // Sunrise/sunset fetch timer (starts after NTP success)
    timers.create(Globals::sunRefreshIntervalMs, 0, cb_fetchSunrise);

    return true;
}

namespace FetchController {

void requestNtpResync() {
    // Reset NTP status, re-enable timer
    prtClock.setTimeFetched(false);
    timers.restart(Globals::clockBootstrapIntervalMs, Globals::wifiRetryCount, cb_fetchNTP, Globals::wifiRetryGrowth);
}

} // namespace FetchController
