# WiFiController

> Version: 260319A | Updated: 2026-03-19

Manages WiFi connection with automatic retry and health monitoring, plus HTTP fetch for NTP/weather/sunrise APIs and NAS CSV backup.

## Files

| File | Purpose |
|------|---------|
| `WiFiController.h/.cpp` | WiFi connect/retry/health, AP fallback |
| `WiFiManager.cpp` | WiFi lifecycle management (implementation-only, no .h) |
| `FetchController.h/.cpp` | HTTP fetch for weather/sunrise APIs and NTP time |
| `FetchManager.cpp` | Fetch lifecycle management (implementation-only, no .h) |
| `NasBackup.h/.cpp` | Push pattern/color CSVs to NAS after save |

## Architecture

WiFiController is a **Controller** layer module — it owns the WiFi hardware driver
and exposes a simple boot API. FetchController handles HTTP fetches. NasBackup pushes CSVs to NAS.

## API

```cpp
// WiFiController.h
void bootWiFiConnect();        // Start connection sequence (call once at boot)

// FetchController.h
bool bootFetchController();    // Initialize fetch subsystem
namespace FetchController { void requestNtpResync(); }

// NasBackup.h
namespace NasBackup {
    void requestPush(const char* filename);  // Queue CSV push to NAS
    void checkHealth();                       // Verify NAS reachability
    void startHealthTimer();                  // Start periodic health check
}
```

## Connection Flow

1. `bootWiFiConnect()` → `beginConnect()`
2. Poll timer (250ms) checks `WiFi.status()` continuously
3. Retry timer uses growing interval: starts 2s, grows 1.5× per retry
4. After configured retries → gives up
5. On connect → cancel retries, start health check timer (5s)
6. Health check detects disconnect → restarts connection sequence

## Timers

| Callback | Interval | Purpose |
|----------|----------|---------|
| `cb_checkWiFiStatus` | 250ms continuous | Poll WiFi.status() |
| `cb_retryConnect` | 2s growing | Retry WiFi.begin() on failure |

## State

- `connected` — module state
- `stationConfigured` — one-time hardware init flag
- `loggedStart` — log-once flag for connection start message

## Configuration

Static IP configured via `HWconfig.h`:
- `USE_STATIC_IP` — enable/disable
- `STATIC_IP_ADDRESS`, `STATIC_GATEWAY`, `STATIC_SUBNET`, `STATIC_DNS`

WiFi credentials from `HWconfig.h`:
- `WIFI_SSID`, `WIFI_PASSWORD`
