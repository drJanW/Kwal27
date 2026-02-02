# Web Interface REST API (Post-Refactor)

_Gebaseerd op: webgui_refactor_plan.md_  
_Status: NA IMPLEMENTATIE_

## 1. Conventies

1. **Transport** – HTTP port 80, geen authenticatie (netwerk perimeter zorgt voor access control)
2. **Content types** – `text/plain` voor responses, `application/json` voor SSE en CRUD payloads
3. **Cache** – `Cache-Control: no-store` op alle JSON responses
4. **Error codes** –
   - `400` → malformed parameters / validation error
   - `404` → SD path not found
   - `500` → internal failure
  - `503` → SD card busy of TodayState unavailable

## 2. SSE Events (primaire communicatie)

Alle state communicatie verloopt via Server-Sent Events. Geen polling endpoints meer.

| Event | Trigger | Payload | Grootte |
|-------|---------|---------|---------|
| `state` | Connect + elke setter | WebGuiStatus JSON | ~200 B |
| `patterns` | Connect + Pattern CRUD | Volledige patterns lijst | ~4 KB |
| `colors` | Connect + Color CRUD | Volledige colors lijst | ~2 KB |

### State Event Payload

```json
{
  "brightness": 128,
  "audioLevel": 0.75,
  "patternId": "rainbow",
  "patternLabel": "Rainbow Fade",
  "colorId": "sunset",
  "colorLabel": "Warm Sunset",
  "fragment": {
    "dir": 3,
    "file": 7,
    "score": 2
  }
}
```

### Bij Connect

Server pusht automatisch in volgorde:
1. `patterns` → volledige lijst
2. `colors` → volledige lijst
3. `state` → actuele WebGuiStatus

Browser krijgt gegarandeerd complete data zonder extra requests.

---

## 3. Static Assets

| Endpoint | Methode | Response |
|----------|---------|----------|
| `/` | GET | `index.html` van SD |
| `/styles.css` | GET | CSS van SD |
| `/kwal.js` | GET | JS bundle van SD |

---

## 4. Brightness & Audio Controls

| Endpoint | Methode | Query/Body | Response | Actie |
|----------|---------|------------|----------|-------|
| `/setBrightness` | **POST** | `value=0..255` | `"OK"` | `WebGuiStatus::setBrightness()` → SSE state |
| `/setWebAudioLevel` | **POST** | `value=0.0..1.0` | `"OK"` | `WebGuiStatus::setAudioLevel()` → SSE state |

**Verwijderd:**
- ~~`GET /getBrightness`~~ → via SSE state
- ~~`GET /getWebAudioLevel`~~ → via SSE state

---

## 5. Audio Endpoints

| Endpoint | Methode | Query | Response | Actie |
|----------|---------|-------|----------|-------|
| `/api/audio/next` | POST | – | `"OK"` | Skip naar volgend fragment |
| `/api/audio/play` | GET | `dir=X` | `"OK"` | Play random uit dir |
| `/api/audio/play` | GET | `dir=X&file=Y` | `"OK"` | Play exact fragment |

**Verwijderd:**
- ~~`GET /api/audio/current`~~ → via SSE state `fragment` object

---

## 6. Voting

| Endpoint | Methode | Query | Response |
|----------|---------|-------|----------|
| `/vote` | POST | `delta=X` (±) | `"VOTE applied dir=X file=Y delta=D score=N"` |
| `/vote` | POST | `delta=0` | `"SCORE dir=X file=Y score=N"` |

Vote met `delta≠0` triggert `WebGuiStatus::setFragmentScore()` → SSE state.

---

## 7. Patterns API

### Lijst ophalen (init/reconnect)

| Endpoint | Methode | Response |
|----------|---------|----------|
| `/api/patterns` | GET | JSON array met patterns + `X-Pattern` header |

Response:
```json
[
  { "id": "rainbow", "label": "Rainbow Fade", "speed": 50, ... },
  { "id": "pulse", "label": "Pulse Wave", "speed": 30, ... }
]
```

Header: `X-Pattern: rainbow` (active pattern ID)

### CRUD operaties

| Endpoint | Methode | Body | Response | Side effect |
|----------|---------|------|----------|-------------|
| `/api/patterns` | POST | `{ label, speed, ... }` | Updated list | → SSE patterns + state |
| `/api/patterns` | POST | `{ id, label, ... }` | Updated list | Update existing → SSE |
| `/api/patterns/select` | POST | `{ "id": "rainbow" }` | `"OK"` | → SSE state |
| `/api/patterns/delete` | POST | `{ "id": "rainbow" }` | Updated list | → SSE patterns + state |
| `/api/patterns/next` | POST | – | `"OK"` | → SSE state |
| `/api/patterns/prev` | POST | – | `"OK"` | → SSE state |
| `/api/patterns/preview` | POST | `{ params... }` | `{ "status": "ok" }` | Transient, geen SSE |

**Create payload (geen id):**
```json
{ "label": "Mijn Pattern", "speed": 50, "preset": 2, "select": true }
```

**Update payload (met id):**
```json
{ "id": "existing", "label": "Nieuwe Naam", "speed": 60 }
```

---

## 8. Colors API

### Lijst ophalen (init/reconnect)

| Endpoint | Methode | Response |
|----------|---------|----------|
| `/api/colors` | GET | JSON array met colors + `X-Color` header |

Response:
```json
[
  { "id": "sunset", "label": "Warm Sunset", "colorA_hex": "#FF7F00", "colorB_hex": "#552200" },
  { "id": "ocean", "label": "Ocean Blue", "colorA_hex": "#0066FF", "colorB_hex": "#003366" }
]
```

Header: `X-Color: sunset` (active color ID)

### CRUD operaties

| Endpoint | Methode | Body | Response | Side effect |
|----------|---------|------|----------|-------------|
| `/api/colors` | POST | `{ label, colorA_hex, ... }` | Updated list | → SSE colors + state |
| `/api/colors` | POST | `{ id, label, ... }` | Updated list | Update existing → SSE |
| `/api/colors/select` | POST | `{ "id": "sunset" }` | `"OK"` | → SSE state |
| `/api/colors/delete` | POST | `{ "id": "sunset" }` | Updated list | → SSE colors + state |
| `/api/colors/next` | POST | – | `"OK"` | → SSE state |
| `/api/colors/prev` | POST | – | `"OK"` | → SSE state |
| `/api/colors/preview` | POST | `{ colorA_hex, colorB_hex }` | `{ "status": "ok" }` | Transient, geen SSE |

---

## 9. SD Card Endpoints

| Endpoint | Methode | Payload | Response |
|----------|---------|---------|----------|
| `/api/sd/status` | GET | – | `{"ready":true,"busy":false,"hasIndex":true}` |
| `/api/sd/upload` | POST | Multipart form | `{"status":"ok","path":"/..."}` |
| `/api/sd/delete` | POST | `{ "path": "/foo/bar" }` | `{"status":"ok"}` |

CSV upload triggert catalog reload → SSE patterns/colors indien relevant.

---

## 10. OTA Endpoints

| Endpoint | Methode | Response |
|----------|---------|----------|
| `/ota/arm` | GET | `"OK"` (arms 300s window) |
| `/ota/confirm` | POST | `200` of `400 EXPIRED` |
| `/ota/start` | POST | `200` (arm + confirm combo) |

---

## 11. Status & Health

| Endpoint | Methode | Response |
|----------|---------|----------|
| `/api/health` | GET | System diagnostics JSON |
| `/api/context/today` | GET | TodayState snapshot |

### /api/health Response (v260104+)

```json
{
  "firmware": "260104M",
  "timers": 12,
  "maxTimers": 60,
  "health": 2047,
  "boot": 0,
  "absent": 100
}
```

| Field | Type | Description |
|-------|------|-------------|
| `firmware` | string | Firmware version |
| `timers` | int | Active timer slots |
| `maxTimers` | int | Total timer capacity |
| `health` | uint16 | Bitmask: 1=OK for each component |
| `boot` | uint64 | 4-bit fields: retries remaining per component |
| `absent` | uint16 | **NEW**: Bitmask: 1=hardware not present per HWconfig |

#### Component Bit Positions

| Bit | Component | HWconfig guard |
|-----|-----------|----------------|
| 0 | SD | - |
| 1 | WiFi | - |
| 2 | RTC | `RTC_PRESENT` |
| 3 | Audio | - |
| 4 | Distance | `DISTANCE_SENSOR_PRESENT` |
| 5 | Lux | `LUX_SENSOR_PRESENT` |
| 6 | Sensor3 | `SENSOR3_PRESENT` |
| 7 | NTP | - |
| 8 | Weather | - |
| 9 | Calendar | - |
| 10 | TTS | - |

#### WebGUI Status Display

- `✅` = OK (health bit set)
- `❌` = Failed (health bit clear, absent bit clear)
- `⟳N` = Init in progress (N retries remaining)
- `—` = Hardware absent (absent bit set)

**Verwijderd:**
- ~~`GET /api/light/status`~~ → via SSE state (patternId/colorId)

---

## 12. Endpoint Summary

### Actieve Endpoints (26 total)

| Categorie | Endpoints |
|-----------|-----------|
| Static | `/`, `/styles.css`, `/kwal.js` |
| Brightness | `POST /setBrightness` |
| Audio | `POST /setWebAudioLevel`, `POST /api/audio/next`, `GET /api/audio/play`, `POST /vote` |
| Patterns | `GET /api/patterns`, `POST /api/patterns`, `POST /api/patterns/select`, `POST /api/patterns/delete`, `POST /api/patterns/next`, `POST /api/patterns/prev`, `POST /api/patterns/preview` |
| Colors | `GET /api/colors`, `POST /api/colors`, `POST /api/colors/select`, `POST /api/colors/delete`, `POST /api/colors/next`, `POST /api/colors/prev`, `POST /api/colors/preview` |
| SD | `GET /api/sd/status`, `POST /api/sd/upload`, `POST /api/sd/delete` |
| OTA | `GET /ota/arm`, `POST /ota/confirm`, `POST /ota/start` |
| Status | `GET /api/health`, `GET /api/context/today` |

### Verwijderde Endpoints (4)

| Endpoint | Vervanger |
|----------|-----------|
| `GET /getBrightness` | SSE state `brightness` |
| `GET /getWebAudioLevel` | SSE state `audioLevel` |
| `GET /api/audio/current` | SSE state `fragment` |
| `GET /api/light/status` | SSE state `patternId`/`colorId` |

### Gewijzigde Endpoints (2)

| Endpoint | Was | Wordt |
|----------|-----|-------|
| `/setBrightness` | GET | **POST** |
| `/setWebAudioLevel` | GET | **POST** |

---

## 13. JavaScript Client Pattern

```javascript
// SSE setup
const es = new EventSource('/events');

es.addEventListener('state', (e) => {
    const data = JSON.parse(e.data);
    // Update UI: brightness, audioLevel, patternId, colorId, fragment
});

es.addEventListener('patterns', (e) => {
    const patterns = JSON.parse(e.data);
    // Cache patterns list
});

es.addEventListener('colors', (e) => {
    const colors = JSON.parse(e.data);
    // Cache colors list
});

// Setters (fire-and-forget)
fetch('/setBrightness?value=128', { method: 'POST' });
fetch('/setWebAudioLevel?value=0.75', { method: 'POST' });
fetch('/api/patterns/select', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ id: 'rainbow' })
});
```

---

## 14. Migratie Notes

### Voor JavaScript

| Verwijder | Vervang door |
|-----------|--------------|
| `Kwal.brightness.load()` | SSE `onState` |
| `Kwal.audio.loadCurrent()` | SSE `onState` |
| `Kwal.sse.onFragment()` | SSE `onState` |
| `Kwal.sse.onLight()` | SSE `onState` |
| `status.js` | Alles in `onState` handler |

### Voor Firmware Handlers

| Was | Wordt |
|-----|-------|
| Direct response met waarde | `"OK"` + SSE push |
| Callback naar SseController | `WebGuiStatus::pushState()` |

---

_Zie webgui_refactor_plan.md voor implementatie details._
