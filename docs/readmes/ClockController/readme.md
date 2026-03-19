# ClockController

> Version: 260319A | Updated: 2026-03-19

Real-time clock management with DS3231 RTC hardware and NTP synchronization.

## Files

| File | Version | Purpose |
|------|---------|---------|
| `PRTClock.h/.cpp` | 260316L | RTC management: time/date getters/setters, sunrise/sunset, moon phase, text formatting |
| `RTCController.h/.cpp` | 260216A | DS3231 hardware driver: I2C read/write, power-loss detection, temperature |

## Hardware

- **Module**: DS3231 For PI (XY-597) mini RTC
- **I2C address**: 0x68
- **Power**: 3.3V - 5V
- **Backup cell**: LIR2032 (rechargeable) — do not replace with CR2032
- **Connector** (left to right, component side): VCC, SDA, SCL, GND, 32K

## API

### PRTClock
```cpp
class PRTClock {
    void begin();
    void update();

    // Time
    uint8_t getHour() / setHour(uint8_t);
    uint8_t getMinute() / setMinute(uint8_t);
    uint8_t getSecond() / setSecond(uint8_t);
    void setTime(uint8_t h, uint8_t m, uint8_t s);

    // Date
    uint16_t getYear() / setYear(uint16_t);
    uint8_t getMonth() / setMonth(uint8_t);
    uint8_t getDay() / setDay(uint8_t);
    bool hasValidDate();
    uint8_t getDoW() / setDoW(uint8_t);
    uint16_t getDoY() / setDoY(uint16_t);

    // Sun
    uint8_t getSunriseHour() / setSunriseHour(uint8_t);
    uint8_t getSunriseMinute() / setSunriseMinute(uint8_t);
    uint8_t getSunsetHour() / setSunsetHour(uint8_t);
    uint8_t getSunsetMinute() / setSunsetMinute(uint8_t);

    // Moon
    uint8_t getMoonPhaseValue() / setMoonPhaseValue(uint8_t);

    // Status
    bool isTimeFetched() const;
    void setTimeFetched(bool value);
    void showTimeDate() const;

    // Text formatting
    static String buildTimeText(uint8_t hour24, uint8_t minute, TimeStyle style);
    static String buildDateText(uint8_t day, uint8_t month, uint16_t year, TimeStyle style);
    String buildTimeSentence(TimeStyle style) const;
    String buildNowSentence(TimeStyle style) const;
};
```

### RTCController (namespace)
```cpp
void begin();
bool isAvailable();
bool readInto(PRTClock& clock);     // Full read (time + date)
bool readTime(PRTClock& clock);     // Time only
void writeFrom(const PRTClock& clock);
float getTemperature();
bool wasPowerLost();
```

## Integration

- `ClockBoot` (in RunManager/Clock/) initializes PRTClock and RTCController at boot
- `ClockRun` manages NTP sync and daily reboot scheduling
- `ContextController` uses PRTClock for time-of-day and sunrise/sunset data
- `SpeakRun` uses `buildTimeSentence()` for TTS time announcements