# Web Interface REST API

> Version: 260409G | Updated: 2026-04-09

## 1. Conventions

1. **Transport** — HTTP port 80, no authentication (network perimeter provides access control)
2. **Content types** — `text/plain` for simple responses, `application/json` for SSE and structured payloads
3. **Cache** — `Cache-Control: no-store` on JSON responses
4. **Error codes** — 400 (bad params), 404 (not found), 500 (internal), 503 (SD busy)

## 2. SSE Events

All state communication via Server-Sent Events at `/api/events`.

| Event | Trigger | Payload |
|-------|---------|---------|
| `state` | Connect + any setter | WebGuiStatus JSON |
| `pushPatterns` | Connect + pattern change | Patterns list |
| `pushColors` | Connect + colors change | Colors list |
| `pushLuxcal` | Lux calibration update | Calibration data |
| `pushLuxcalFit` | Fit result | Fit parameters |

### State Event Payload

```json
{
  "sliderPct": 75,
  "brightnessLo": 70,
  "brightnessHi": 242,
  "brightnessMax": 242,
  "audioSliderPct": 65,
  "volumeLo": 0.05,
  "volumeHi": 0.37,
  "volumeMax": 0.47,
  "patternId": "rainbow",
  "patternLabel": "Rainbow Fade",
  "colorId": "sunset",
  "colorLabel": "Warm Sunset",
  "fragment": {
    "dir": 3,
    "file": 7,
    "score": 2,
    "durationMs": 45000,
    "boxName": "Lente"
  },
  "silence": false,
  "speakMin": 30,
  "fragMin": 10,
  "durMin": 20000,
  "tvMode": false,
  "hasLuxSensor": true,
  "sleepArmed": false
}
```

### On Connect

Server pushes in order:
1. `pushPatterns` — full pattern list
2. `pushColors` — full colors list
3. `state` — current WebGuiStatus

## 3. Static Assets

| Endpoint | Method | Response |
|----------|--------|----------|
| `/` | GET | index.html from SD |
| `/favicon.ico` | GET | 204 No Content |
| `/styles.css` | GET | CSS from SD (served by SD handler) |
| `/kwal.js` | GET | JS bundle from SD (served by SD handler) |

## 4. Brightness & Audio Controls

| Endpoint | Method | Params | Response | Action |
|----------|--------|--------|----------|--------|
| `/setBrightness` | GET/POST | `value=0..100` | `"OK"` | Set brightness slider |
| `/getBrightness` | GET | — | brightness value | Query current |
| `/setWebAudioLevel` | GET/POST | `value=0..100` | `"OK"` | Set audio slider |
| `/getWebAudioLevel` | GET | — | audio level | Query current |

## 5. Audio Endpoints

| Endpoint | Method | Params | Response |
|----------|--------|--------|----------|
| `/api/audio/next` | POST | — | `"OK"` — skip to next fragment |
| `/api/audio/play` | GET | `dir=X` | Play random from dir |
| `/api/audio/play` | GET | `dir=X&file=Y` | Play exact fragment |
| `/api/audio/current` | GET | — | Current fragment info |
| `/api/audio/themebox` | GET | — | Current theme box info |
| `/api/audio/playbox` | GET | — | Play from current theme box |
| `/api/audio/grid` | GET | — | Audio grid data |
| `/api/audio/intervals` | GET/POST | GET: query, POST: set | Fragment/speak intervals |
| `/api/audio/silence` | GET/POST | GET: query, POST: toggle | Silence mode |
| `/api/audio/freetext` | POST | `text`, `interval`, `duration` | Start/clear free text TTS |

### Free Text TTS (`/api/audio/freetext`)

Submit arbitrary Dutch text for TTS playback with optional repeat.

**Parameters:**
- `text` — String (max 160 chars, recommended ≤99). Empty string = clear/stop.
- `interval` — Repeat interval in minutes (0 = once only).
- `duration` — Total duration in minutes (repeat count = duration / interval).

**Behavior:**
- Non-empty text: downloads VoiceRSS MP3 → caches to SD `/127/000.mp3` → plays via PlayFragment → optional repeat timer from SD cache.
- Empty text or clear: cancels timers, stops repeat.
- Web handler is memory-only; actual download and playback deferred to timer callback.
- Characters `? ! / \ @ # € &` etc. are all supported (URL-encoded by PlaySentence).

## 6. Patterns API

| Endpoint | Method | Response |
|----------|--------|----------|
| `/api/patterns` | GET | JSON array + `X-Pattern` header (active ID) |
| `/api/patterns/next` | POST | `"OK"` — advance to next pattern |
| `/api/patterns/prev` | POST | `"OK"` — go to previous pattern |

## 7. Colors API

| Endpoint | Method | Response |
|----------|--------|----------|
| `/api/colors` | GET | JSON array + `X-Color` header (active ID) |
| `/api/colors/next` | POST | `"OK"` — advance to next colors |
| `/api/colors/prev` | POST | `"OK"` — go to previous colors |

## 8. Voting

| Endpoint | Method | Params | Response |
|----------|--------|--------|----------|
| `/vote` | ANY | `delta=N` (+/-) | `"VOTE applied dir=X file=Y delta=D score=N"` |
| `/vote` | ANY | `delta=0` | `"SCORE dir=X file=Y score=N"` (query only) |

## 9. Lux Calibration

| Endpoint | Method | Response |
|----------|--------|----------|
| `/api/lux/calibrate` | POST | Start calibration |
| `/api/lux/sample` | POST | Add calibration sample |
| `/api/lux/status` | GET | Current calibration state |
| `/api/lux/fit` | POST | Run curve fit |
| `/api/lux/accept` | POST | Accept fit result |
| `/api/lux/clear` | POST | Clear samples (keep seeds) |
| `/api/lux/reset` | POST | Full reset |
| `/api/lux/csv` | GET | Download calibration CSV |
| `/api/lux/points` | GET | Get calibration data points |
| `/api/lux/reload` | POST | Reload from SD |

## 10. Health & System

| Endpoint | Method | Response |
|----------|--------|----------|
| `/api/health` | GET | Health status JSON (component bits) |
| `/api/restart` | POST | Restart ESP32 |
| `/api/sleep` | POST | Arm sleep timer |
| `/api/sleep/cancel` | POST | Cancel sleep timer |
| `/api/wifi/config` | POST | Update WiFi credentials |

## 11. SD Card

| Endpoint | Method | Params | Response |
|----------|--------|--------|----------|
| `/api/sd/status` | GET | — | `{"ready":true,"busy":false}` |
| `/api/sd/list` | GET | path | Directory listing |
| `/api/sd/file` | GET | path | File download |
| `/api/sd/delete` | POST | path | Delete file |
| `/api/sd/upload` | POST | multipart | Upload file |
| `/api/sd/rebuild` | POST | — | Rebuild SD index |
| `/api/sd/syncdir` | POST | — | Sync directory |
| `/api/sd/syncstart` | POST | — | Start sync mode |
| `/api/sd/syncstop` | POST | — | Stop sync mode |

## 12. OTA

| Endpoint | Method | Response |
|----------|--------|----------|
| `/ota/upload` | POST | Firmware upload (multipart) |

## 13. Context

| Endpoint | Method | Response |
|----------|--------|----------|
| `/api/context/today` | GET | Current TodayState JSON |

## 14. TV Mode

| Endpoint | Method | Response |
|----------|--------|----------|
| `/api/tvmode` | GET | Activate TV simulator |
| `/api/tvstop` | GET | Stop TV simulator |

## 15. Logging

| Endpoint | Method | Response |
|----------|--------|----------|
| `/log` | GET | Current log buffer |
| `/log/clear` | GET | Clear log buffer |
