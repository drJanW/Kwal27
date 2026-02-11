/**
 * @file PRTClock.h
 * @brief Real-time clock management with DS3231 RTC and NTP synchronization
 * @version 260202A
 $12026-02-05
 */
#pragma once
#include <Arduino.h>

enum class TimeStyle {
  FORMAL,
  NORMAL,
  INFORMAL
};

bool fetchCurrentTime();

class PRTClock {
public:
  PRTClock(const PRTClock&) = delete;
  PRTClock& operator=(const PRTClock&) = delete;

  void begin();
  void update();

  // time
  uint8_t  getHour()   const;
  void     setHour(uint8_t v);
  uint8_t  getMinute() const;
  void     setMinute(uint8_t v);
  uint8_t  getSecond() const;
  void     setSecond(uint8_t v);
  void     setTime(uint8_t hh, uint8_t mm, uint8_t ss);

  // date
  uint8_t  getYear()   const;            // 0..99 or 2000+ if you prefer
  void     setYear(uint8_t v);
  uint8_t  getMonth()  const;
  void     setMonth(uint8_t v);
  uint8_t  getDay()    const;
  void     setDay(uint8_t v);
  bool     hasValidDate() const;
  uint8_t  getDoW()    const;
  void     setDoW(uint8_t v);
  void     setDoW(uint8_t y, uint8_t m, uint8_t d);
  uint16_t getDoY()    const;
  void     setDoY(uint8_t y, uint8_t m, uint8_t d);

  // sunrise/sunset
  uint8_t  getSunriseHour()   const;
  void     setSunriseHour(uint8_t v);
  uint8_t  getSunriseMinute() const;
  void     setSunriseMinute(uint8_t v);
  uint8_t  getSunsetHour()    const;
  void     setSunsetHour(uint8_t v);
  uint8_t  getSunsetMinute()  const;
  void     setSunsetMinute(uint8_t v);

  // moon
  float    getMoonPhaseValue() const;
  void     setMoonPhaseValue();

  bool     isTimeFetched() const;
  void     setTimeFetched(bool value);

  void     showTimeDate() const;

  static String buildTimeText(uint8_t hour24, uint8_t minute, TimeStyle style = TimeStyle::NORMAL);
  static String buildDateText(uint8_t day, uint8_t month, uint16_t year = 0, TimeStyle style = TimeStyle::NORMAL);
  String buildTimeSentence(TimeStyle style = TimeStyle::NORMAL) const;
  String buildNowSentence(TimeStyle style = TimeStyle::NORMAL) const;

  PRTClock() = default;
};

extern PRTClock prtClock;
