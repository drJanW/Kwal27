# WiFiController

> Version: 260205D | Updated: 2026-02-05

Manages WiFi connection with automatic retry and health monitoring.

## Architecture

WiFiController is a **Controller** layer module - it owns the WiFi hardware driver
and exposes a simple query API. No Run/Policy layers needed.

## API

```cpp
void bootWiFiConnect();    // Start connection sequence (call once at boot)
bool isWiFiConnected();    // Query current connection state
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
| `cb_healthCheck` | 5s continuous | Detect connection loss |

## State

- `connected` - module state, accessible via `isWiFiConnected()`
- `stationConfigured` - one-time hardware init flag
- `loggedStart` - log-once flag for connection start message

## Configuration

Static IP configured via `HWconfig.h`:
- `USE_STATIC_IP` - enable/disable
- `STATIC_IP_ADDRESS`, `STATIC_GATEWAY`, `STATIC_SUBNET`, `STATIC_DNS`

WiFi credentials from `HWconfig.h`:
- `WIFI_SSID`, `WIFI_PASSWORD`
