/**
 * @file PRTClock.cpp
 * @brief Real-time clock management with DS3231 RTC and NTP synchronization
 * @version 251231E
 * @date 2025-12-31
 *
 * This implementation provides the PRTClock singleton that manages timekeeping
 * for the system. It uses atomic storage for all time/date fields to ensure
 * thread-safe access. The class handles:
 * - Time tracking with automatic second/minute/hour rollover via update()
 * - DS3231 RTC hardware communication for persistent timekeeping
 * - NTP synchronization to fetch accurate time from the network
 * - Sunrise/sunset calculation based on geographic coordinates
 * - Moon phase computation for astronomical features
 * - Daily automatic re-sync at midnight to maintain accuracy
 */

#include <Arduino.h>
#include <atomic>
#include <math.h>
#include "PRTClock.h"
#include "FetchManager.h"
#include "Globals.h"
#include "LogBuffer.h"

// --- Atomic storage for all fields ---
static std::atomic<uint8_t>  _valHour{0}, _valMinute{0}, _valSecond{0};
static std::atomic<uint8_t>  _valYear{0}, _valMonth{1}, _valDay{1}, _valDoW{0};
static std::atomic<uint16_t> _valDoY{1};
static std::atomic<uint8_t>  _valSunriseHour{0}, _valSunriseMinute{0};
static std::atomic<uint8_t>  _valSunsetHour{0},  _valSunsetMinute{0};
static std::atomic<float>    _valMoonPhase{0.0f};
static std::atomic<bool>     _timeFetched{false};

// Timestamp provider for logging macros (uses atomics directly to avoid recursion)
static bool provideTimestamp(char* buf, size_t bufSize) {
    if (bufSize < 10) return false;
    snprintf(buf, bufSize, "%02u:%02u:%02u ", 
             _valHour.load(std::memory_order_relaxed),
             _valMinute.load(std::memory_order_relaxed),
             _valSecond.load(std::memory_order_relaxed));
    return true;
}

PRTClock& PRTClock::instance() {
  static PRTClock inst;
  static bool providerSet = false;
  if (!providerSet) {
      LogBuffer::setTimestampProvider(provideTimestamp);
      providerSet = true;
  }
  return inst;
}

// ===================================================
// Lifecycle
// ===================================================
void PRTClock::update() {
  // advance one second
  uint8_t ss = getSecond();
  uint8_t mm = getMinute();
  uint8_t hh = getHour();

  if (++ss >= 60) {
    ss = 0;
    if (++mm >= 60) {
      mm = 0;
      if (++hh >= 24) hh = 0;
    }
  }

  setSecond(ss);
  setMinute(mm);
  setHour(hh);

  // Date rollover at midnight
  static uint8_t lastHour = 255;
  if (lastHour == 23 && hh == 0) {
    // Advance date
    uint8_t d = getDay();
    uint8_t m = getMonth();
    uint8_t y = getYear();
    uint8_t daysInMonth[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    if (m == 2 && (y % 4 == 0)) daysInMonth[1] = 29; // leap year
    if (++d > daysInMonth[m-1]) {
      d = 1;
      if (++m > 12) { m = 1; y++; }
      setMonth(m);
      setYear(y);
    }
    setDay(d);
    PF("[PRTClock] Date rollover to %u-%02u-%02u\n", 2000+y, m, d);
  }
  lastHour = hh;

  // once per midnight, trigger re-sync
  static bool synced_today = false;
  if (hh == 0 && mm == 0 && ss > 1 && !synced_today) {
    // Reset flag - FetchManager::requestNtpResync() will restart timer
    setTimeFetched(false);
    FetchManager::requestNtpResync();

    setDoW(getYear(), getMonth(), getDay());
    setDoY(getYear(), getMonth(), getDay());
    setMoonPhaseValue();

    synced_today = true;
  }
  if (hh == 1 && mm == 0 && ss > 0) {
    synced_today = false; // re-arm for next midnight
  }
}

// ===================================================
// Time setters/getters
// ===================================================
uint8_t PRTClock::getHour() const   { return getMux(&_valHour); }
void    PRTClock::setHour(uint8_t v){ setMux(v, &_valHour); }

uint8_t PRTClock::getMinute() const { return getMux(&_valMinute); }
void    PRTClock::setMinute(uint8_t v){ setMux(v, &_valMinute); }

uint8_t PRTClock::getSecond() const { return getMux(&_valSecond); }
void    PRTClock::setSecond(uint8_t v){ setMux(v, &_valSecond); }

void PRTClock::setTime(uint8_t hh, uint8_t mm, uint8_t ss) {
  setHour(hh); setMinute(mm); setSecond(ss);
}

// ===================================================
// Date setters/getters
// ===================================================
uint8_t PRTClock::getYear() const   { return getMux(&_valYear); }
void    PRTClock::setYear(uint8_t v){ setMux(v, &_valYear); }

uint8_t PRTClock::getMonth() const  { return getMux(&_valMonth); }
void    PRTClock::setMonth(uint8_t v){ setMux(v, &_valMonth); }

uint8_t PRTClock::getDay() const    { return getMux(&_valDay); }
void    PRTClock::setDay(uint8_t v) { setMux(v, &_valDay); }

bool PRTClock::hasValidDate() const {
  return getYear() != 0 && getMonth() != 0 && getDay() != 0;
}

uint8_t PRTClock::getDoW() const    { return getMux(&_valDoW); }
void    PRTClock::setDoW(uint8_t v) { setMux(v, &_valDoW); }

void PRTClock::setDoW(uint8_t y, uint8_t m, uint8_t d) {
  if (m < 3) { m += 12; y -= 1; }
  uint8_t K = y % 100, J = y / 100;
  uint16_t h = (d + 13 * (m + 1) / 5 + K + K/4 + J/4 + 5*J) % 7;
  setDoW((uint8_t)((h + 6) % 7));
}

uint16_t PRTClock::getDoY() const   { return getMux(&_valDoY); }
void     PRTClock::setDoY(uint8_t y, uint8_t m, uint8_t d) {
  uint16_t doy = d;
  for (uint8_t i = 1; i < m; ++i) {
    switch (i) {
      case 1: case 3: case 5: case 7: case 8: case 10: case 12: doy += 31; break;
      case 4: case 6: case 9: case 11: doy += 30; break;
      case 2: doy += (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0)) ? 29 : 28; break;
    }
  }
  setMux(doy, &_valDoY);
}

// ===================================================
// Sunrise / Sunset
// ===================================================
uint8_t PRTClock::getSunriseHour() const   { return getMux(&_valSunriseHour); }
void    PRTClock::setSunriseHour(uint8_t v){ setMux(v, &_valSunriseHour); }

uint8_t PRTClock::getSunriseMinute() const { return getMux(&_valSunriseMinute); }
void    PRTClock::setSunriseMinute(uint8_t v){ setMux(v, &_valSunriseMinute); }

uint8_t PRTClock::getSunsetHour() const    { return getMux(&_valSunsetHour); }
void    PRTClock::setSunsetHour(uint8_t v) { setMux(v, &_valSunsetHour); }

uint8_t PRTClock::getSunsetMinute() const  { return getMux(&_valSunsetMinute); }
void    PRTClock::setSunsetMinute(uint8_t v){ setMux(v, &_valSunsetMinute); }

// ===================================================
// Moon phase
// ===================================================
float PRTClock::getMoonPhaseValue() const { return getMux(&_valMoonPhase); }

static float calcMoon() {
  auto &clock = PRTClock::instance();
  int y = clock.getYear(), m = clock.getMonth(), d = clock.getDay();
  if (m <= 2) { y--; m += 12; }
  const float jd0 = 2451550.1f, cycle = 29.53058867f;
  int A = y / 100, B = 2 - A + (A / 4);
  float jd = int(365.25f * (y + 4716)) + int(30.6001f * (m + 1)) + d + B - 1524.5f;
  float phase = fmodf(jd - jd0, cycle);
  if (phase < 0) phase += cycle;
  return 100.0f * 0.5f * (1.0f - cosf(2.0f * float(M_PI) * (phase / cycle)));
}

void PRTClock::setMoonPhaseValue() { setMux(calcMoon(), &_valMoonPhase); }

// ===================================================
// Debug
// ===================================================
void PRTClock::showTimeDate() const {
  PF(" Time: %02u:%02u:%02u (%u-%02u-%02u)\n",
     getHour(), getMinute(), getSecond(),
     (unsigned)(2000 + getYear()), getMonth(), getDay());
}

namespace {

String periodOfDay(uint8_t hour) {
  if (hour < 6) return String(" snachts");
  if (hour < 12) return String(" sochtends");
  if (hour < 18) return String(" smiddags");
  return String(" savonds");
}
const char *weekdayName(uint8_t dow) {
  static const char *names[] = {
      "zondag", "maandag", "dinsdag",
      "woensdag", "donderdag", "vrijdag", "zaterdag"};
  return (dow < 7) ? names[dow] : "";
}

const char *monthName(uint8_t month) {
  static const char *names[] = {
      "januari", "februari", "maart", "april",
      "mei", "juni", "juli", "augustus",
      "september", "oktober", "november", "december"};
  return (month >= 1 && month <= 12) ? names[month - 1] : "";
}

String periodPhrase(uint16_t mins) {
  if (mins < 330) return String(" snachts");
  if (mins < 501) return String(" smorgens");
  if (mins < 990) return String();
  if (mins < 1080) return String(" smiddags");
  return String(" savonds");
}

void appendWord(String &target, const String &fragment) {
  if (!target.isEmpty()) target += ' ';
  target += fragment;
}

} // namespace

bool PRTClock::isTimeFetched() const {
  return _timeFetched.load(std::memory_order_relaxed);
}

void PRTClock::setTimeFetched(bool value) {
  _timeFetched.store(value, std::memory_order_relaxed);
}

String PRTClock::buildTimeText(uint8_t hour24, uint8_t minute, TimeStyle style) {
  hour24 %= 24;
  minute %= 60;

  uint8_t baseHour12 = hour24 % 12;
  if (baseHour12 == 0) baseHour12 = 12;
  String sBaseHour12 = String(baseHour12);
  uint8_t nextHour12 = static_cast<uint8_t>((baseHour12 % 12) + 1);
  String sNextHour12 = String(nextHour12);

  String t;
  switch (style) {
    case TimeStyle::FORMAL: {
      t.reserve(24);
      t += String(hour24);
      t += " uur";
      if (minute > 0) {
        t += ' ';
        t += String(minute);
      }
      break;
    }

    case TimeStyle::NORMAL: {
      if (minute == 0) {
        t = sBaseHour12 + " uur";
      } else if (minute == 15) {
        t = String("kwart over ") + sBaseHour12;
      } else if (minute == 30) {
        t = String("half ") + sNextHour12;
      } else if (minute == 45) {
        t = String("kwart voor ") + sNextHour12;
      } else if (minute < 30) {
        t = String(minute) + " over " + sBaseHour12;
      } else {
        uint8_t diff = static_cast<uint8_t>(60 - minute);
        t = String(diff) + " voor " + sNextHour12;
      }
      t += periodOfDay(hour24);
      break;
    }

    case TimeStyle::INFORMAL: {
      uint8_t hour12 = hour24 % 12;
      if (hour12 == 0) hour12 = 12;
      uint8_t nextHour = static_cast<uint8_t>((hour12 % 12) + 1);
      uint16_t mins = static_cast<uint16_t>(hour24) * 60U + minute;
      String prefix;
      if ((static_cast<uint16_t>(11 + minute) % 37U) == 0U) appendWord(prefix, String("volgens mij"));
      if ((static_cast<uint16_t>(23 + minute) % 39U) == 0U) appendWord(prefix, String("iets van"));
      if ((static_cast<uint16_t>(37 + minute) % 43U) == 0U) appendWord(prefix, String("zo’n beetje"));

      const String per = periodPhrase(mins);
      switch (minute) {
        case 0:
          if (((hour12 + mins) % 3U) == 0U) {
            t = String("exact ") + String(hour12) + " uur" + per;
          } else {
            t = String(hour12) + " uur" + per + " precies";
          }
          break;

        case 1:
          if (((hour12 + mins) % 11U) == 0U) {
            t = String("alweer ") + String(hour12) + " uur" + per + " geweest";
          } else {
            t = String("iets over ") + String(hour12) + " uur" + per;
          }
          break;

        case 2:
          t = String("ruim ") + String(hour12) + " uur" + per + " geweest";
          break;

        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
        case 8:
        case 9:
        case 10:
        case 11:
        case 12:
          if (((hour12 + mins) % 3U) == 0U) {
            t = String(minute) + " minuten over " + String(hour12) + per;
          } else {
            t = String(minute) + " over " + String(hour12) + per;
          }
          break;

        case 13:
          t = String("nog geen kwart over ") + String(hour12) + per;
          break;

        case 14:
          t = String("plusminus kwart over ") + String(hour12) + per;
          break;

        case 15:
          t = String("kwart over ") + String(hour12) + per + " precies";
          break;

        case 16:
          t = String("zo’n kwart over ") + String(hour12) + per;
          break;

        case 17:
          t = String("ruim kwart over ") + String(hour12) + per + " geweest";
          break;

        case 18:
        case 19:
          if (((hour12 + mins) % 13U) == 0U) appendWord(t, String("nu zo’n beetje"));
          appendWord(t, String(minute) + " minuten over " + String(hour12) + per);
          break;

        case 20:
        case 21:
        case 22:
        case 23:
        case 24:
        case 25:
        case 26:
        case 27: {
          if (((hour12 + mins) % 19U) == 0U) appendWord(t, String("nu zo’n beetje"));
          uint8_t diff = static_cast<uint8_t>(30 - minute);
          appendWord(t, String(diff) + " minuten voor half " + String(nextHour) + per);
          break;
        }

        case 28:
          t = String("nog geen half ") + String(nextHour) + per;
          break;

        case 29:
          t = String("bijna half ") + String(nextHour) + per;
          break;

        case 30:
          if (((hour12 + mins) % 7U) == 0U) {
            t = String("half ") + String(nextHour) + per + " precies";
          } else {
            t = String("exact ") + String(nextHour) + per;
          }
          break;

        case 31:
          t = String("om en nabij half ") + String(nextHour) + per;
          break;

        case 32:
          t = String("ruim half ") + String(nextHour) + per;
          break;

        case 33:
        case 34:
        case 35:
        case 36:
        case 37:
        case 38:
        case 39: {
          uint8_t diff = static_cast<uint8_t>(minute - 30);
          t = String(diff) + " over half " + String(nextHour) + per;
          if (((hour12 + mins) % 13U) == 0U) t += " precies";
          break;
        }

        case 40:
        case 41:
        case 42: {
          if (((hour12 + mins) % 17U) == 0U) appendWord(t, String("nu zo’n beetje"));
          uint8_t diff = static_cast<uint8_t>(minute - 30);
          appendWord(t, String(diff) + " over half " + String(nextHour) + per);
          break;
        }

        case 43:
          t = String("zowat kwart voor ") + String(nextHour) + per;
          break;

        case 44:
          t = String("net geen kwart ") + String(nextHour) + per;
          break;

        case 45:
          t = String("kwart voor ") + String(nextHour) + " precies" + per;
          break;

        case 46:
          t = String("zo’n beetje kwart voor ") + String(nextHour) + per + " geweest";
          break;

        case 47:
          t = String("iets van kwart voor ") + String(nextHour) + per;
          break;

        case 48:
        case 49:
        case 50:
        case 51:
        case 52:
        case 53:
        case 54: {
          if (((hour12 + mins) % 21U) == 0U) appendWord(t, String("alweer"));
          uint8_t diff = static_cast<uint8_t>(60 - minute);
          if (((hour12 + mins) % 3U) == 0U) {
            appendWord(t, String(diff) + " minuten voor " + String(nextHour) + per);
          } else {
            appendWord(t, String(diff) + " voor " + String(nextHour) + per);
          }
          break;
        }

        case 55:
          t = String("5 voor ") + String(nextHour) + per;
          break;

        case 56:
          t = String("nog geen ") + String(nextHour) + " uur" + per;
          break;

        case 57:
          t = String("bijna ") + String(nextHour) + " uur" + per;
          break;

        case 58:
          t = String("plusminus ") + String(nextHour) + " uur" + per;
          break;

        case 59:
          t = String("tegen de klok van ") + String(nextHour) + " uur" + per;
          break;
      }

      if (!prefix.isEmpty()) {
        String combined;
        appendWord(combined, prefix);
        appendWord(combined, t);
        return combined;
      }
      break;
    }
  }

  return t;
}

String PRTClock::buildDateText(uint8_t day, uint8_t month, uint16_t year, TimeStyle style) {
  String result;
  result.reserve(32);
  if (day >= 1U && day <= 31U) {
    result += String(day);
    result += ' ';
  }
  result += monthName(month);
  if (year > 0U) {
    if (style != TimeStyle::INFORMAL) {
      result += ' ';
      uint16_t fullYear = year;
      if (year < 100U) fullYear = static_cast<uint16_t>(2000U + year);
      result += String(fullYear);
    }
  }
  return result;
}

String PRTClock::buildTimeSentence(TimeStyle style) const {
  String sentence("Het is ");
  sentence += buildTimeText(getHour(), getMinute(), style);
  return sentence;
}

String PRTClock::buildNowSentence(TimeStyle style) const {
  String sentence("Het is ");
  sentence += weekdayName(getDoW());
  sentence += ' ';
  sentence += buildDateText(getDay(), getMonth(), getYear(), style);
  sentence += ", ";
  sentence += buildTimeText(getHour(), getMinute(), style);
  return sentence;
}

// ===================================================
// Global instance
// ===================================================
