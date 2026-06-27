# ClockController — Real-Time Clock & Time-of-Day Module

**Role in Kwal27:** Manages the DS3231 hardware RTC and all derived time calculations (sunrise/sunset, moon phase, DoW/DoY). Provides the authoritative `PRTClock` state used by the entire system. NTP synchronization is triggered by ContextController/WiFiController.

## Files

### PRTClock.h / PRTClock.cpp
The Kwal27 clock model. Holds all time state: hour, minute, second, year, month, day, day-of-week, day-of-year, sunrise/sunset times, moon phase, weather min/max, and RTC temperature. Also tracks NTP sync status. Provides getters/setters for all fields and computes derived values (DoW from date, DoY, sunrise/sunset via formula). The `TimeStyle` enum (FORMAL/NORMAL/INFORMAL) controls speech style for time announcements.

Global instance: `extern PRTClock prtClock`.

### RTCController.h / RTCController.cpp
Hardware abstraction for the DS3231 I2C RTC chip. Only this module talks directly to I2C/RTC hardware. API:
- `begin()` — Initialize I2C bus and RTC
- `readInto(PRTClock&)` — Load all RTC registers into the clock model
- `readTime(PRTClock&)` — Lightweight read (H:M:S + date only)
- `writeFrom(const PRTClock&)` — Write clock model back to RTC
- `getTemperature()` — Read RTC temperature sensor
- `isAvailable()` / `wasPowerLost()` — Health checks