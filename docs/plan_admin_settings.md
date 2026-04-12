# Plan: Admin Settings Modal — globals.csv editor in WebGUI

**Status:** DRAFT — niet doordacht, open voor discussie  
**Datum:** 2026-04-12

---

## Doel

Alle ~75 parameters uit `globals.csv` bewerkbaar maken via de webgui.  
Na save: CSV naar SD schrijven → ESP restart.

---

## Wat er al IS

| Component | Status | Locatie |
|-----------|--------|---------|
| Modal open/close systeem | Bestaat | `modal.js`, `data-open`/`data-close` |
| PIN-beveiliging (1951) | Bestaat | `HealthRoutes.cpp`, query param `?pin=` |
| `/api/restart` endpoint | Bestaat | `HealthRoutes.cpp`, deferred via timer (500ms) |
| JS module template (IIFE) | Bestaat | Alle `webgui-src/js/*.js` |
| SD-write guards | Bestaat | `isSdBusy()`, `lockSD()`/`unlockSD()` |
| globals.csv parser | Bestaat | `Globals.cpp` → `applyOverride()` if/else chain |

---

## Wat er NIET is (en gebouwd moet worden)

### 1. Firmware: GET `/api/admin/globals` — lees globals als JSON

**Doel:** Stuur alle globals.csv parameters als JSON array naar de browser.

**Aanpak:** Niet uit RAM (dat vereist een reverse-mapping van alle ~75 variabelen),
maar **direct van SD lezen** en elke CSV-regel als JSON object sturen.

```
GET /api/admin/globals?pin=1951

Response: [
  { "section": "AUDIO" },
  { "key": "colorChangeIntervalMs", "type": "u", "value": "20880000", "comment": "pick new random color", "active": true },
  { "key": "brightnessLo",          "type": "u", "value": "16",       "comment": "slider left boundary", "active": true },
  { "key": "oldDisabledParam",      "type": "u", "value": "500",      "comment": "was active",           "active": false },
  ...
]
```

- **Actieve regels** (uncommented) → `"active": true`
- **Commented regels** (`#key;type;value;comment`) → `"active": false`, `#` gestript uit key
- Section headers (`# ═══`) → `{"section": "AUDIO"}` markers → frontend groepeert hierop
- Lege regels worden overgeslagen
- PIN-check als eerste stap, 403 bij fout

**Guard:** `isSdBusy()` check → 409 als SD bezet

**Nieuwe file:** `lib/WebInterfaceController/routes/AdminRoutes.cpp` + `.h`

### 2. Firmware: POST `/api/admin/globals` — schrijf globals.csv naar SD

**Doel:** Ontvang volledige globals als JSON, schrijf als CSV naar SD.

```
POST /api/admin/globals?pin=1951
Content-Type: application/json

[
  { "key": "colorChangeIntervalMs", "type": "u", "value": "20880000", "comment": "pick new random color" },
  ...
]
```

**BESLISPUNT: directe SD-write of deferred?**

- Hard stop 6 zegt: "no SD I/O from a web handler"
- MAAR: de OTA handler (`OtaRoutes.cpp`) schrijft direct naar flash vanuit de handler
- En de SD upload handler (`SdRoutes.cpp`) schrijft files direct vanuit de handler met `lockSD()`
- Voorstel: **directe write met `lockSD()`/`unlockSD()`**, consistent met bestaand SD-upload patroon
- Alternatief: flag + timer callback (strenger, maar meer complex)

**Flow:**
1. PIN check → 403
2. `isSdBusy()` → 409 "SD busy, try again"
3. `lockSD()`
4. Open `/config/globals.csv` voor schrijven
5. Schrijf section headers + data regels in origineel formaat
6. Actieve params: `key;type;value;comment`
7. Disabled params: `#key;type;value;comment`
8. `unlockSD()`
9. Respond 200 `{"status":"saved"}`
10. Client roept direct daarna `/api/restart` aan

**Validatie (streng, firmware-side):**
- `u` (uint32): `strtoul()` succeeds, value ≥ 0
- `f` (float): `strtof()` succeeds, niet NaN/Inf
- `i` (int32): `strtol()` succeeds
- `b` (bool): alleen "0" of "1"
- `s` (string): non-empty
- Lo/Hi paren: Lo ≤ Hi (bijv. `brightnessLo` ≤ `brightnessHi`)
- Intervallen: > 0
- Fracties: 0.0-1.0 waar van toepassing
- Bij validation failure: 400 met `{"error":"key: reason"}`, NIET schrijven

### 3. WebGUI: HTML — admin settings modal

**Locatie:** `sdroot/index.html`

**Toegang:** Via system-settings-modal → nieuwe knop "⚙️ Admin" (naast restart/sleep)

```html
<div id="admin-settings-modal" class="modal">
  <div class="modal-box wide">
    <!-- GEEN ✕ close knop — alleen Save of Cancel als uitweg -->
    <h3>Admin Settings</h3>
    <div id="admin-pin-gate">
      <input type="password" id="admin-pin" placeholder="PIN" maxlength="4" inputmode="numeric">
      <button id="admin-pin-btn" class="btn-ok">Unlock</button>
    </div>
    <div id="admin-content" style="display:none">
      <div id="admin-sections">
        <!-- Dynamisch gevuld per sectie -->
      </div>
      <div class="admin-actions">
        <button id="admin-save-btn" class="btn-warn">💾 Save & Restart</button>
        <button id="admin-cancel-btn" class="btn-warn">❌ Cancel & Restart</button>
      </div>
    </div>
  </div>
</div>
```

**Per parameter — input row:**
```html
<!-- Active parameter -->
<div class="edit-row">
  <input type="checkbox" class="admin-active" data-key="colorChangeIntervalMs" checked>
  <label title="pick new random color">colorChangeIntervalMs</label>
  <input type="text" data-key="colorChangeIntervalMs" data-type="u" value="20880000">
</div>

<!-- Commented/disabled parameter (grayed out) -->
<div class="edit-row disabled">
  <input type="checkbox" class="admin-active" data-key="oldParam">
  <label title="was active">oldParam</label>
  <input type="text" data-key="oldParam" data-type="u" value="500" disabled>
</div>
```

- Checkbox = active/commented toggle. Unchecked = `#` prefix bij save
- **Auto-uncomment:** als value wordt gewijzigd op een disabled row → checkbox auto-checked, row enabled
- Comment als `title` tooltip op het label
- Section headers als collapsible `<h4>` dividers
- `data-type` voor client-side type validatie (type zelf niet getoond)
- Disabled rows: grayed-out tekst + input disabled tot checkbox aangezet

### 4. WebGUI: JavaScript — `adminSettings.js`

**Locatie:** `sdroot/webgui-src/js/adminSettings.js`

```
Kwal.adminSettings = (function() {
  var pin, sections, data;

  function init()      — bind buttons, PIN input
  function unlock()    — fetch GET /api/admin/globals?pin=XXXX → load
  function render(json) — bouw secties + input rows
  function save()      — validate all → POST /api/admin/globals?pin=XXXX → POST /api/restart
  function cancel()    — POST /api/restart (geen save, direct herstart)
  function validate()  — client-side type+logic checks, block save on failure
  function markDirty() — highlight gewijzigde velden (gele border)
  function onValueChange() — als disabled row wordt bewerkt: auto-check checkbox, enable row

  return { init, unlock, save };
})();
```

**Registratie:** In `main.js` → `Kwal.adminSettings.init()`

### 5. WebGUI: CSS — admin-specifieke styles

**Locatie:** `sdroot/styles.css`

- `.admin-section h4` — section header styling
- `#admin-content .edit-row label` — breder dan standaard (key names zijn lang)
- `#admin-content .edit-row input` — monospace voor getallen
- `.dirty` class — highlight gewijzigde velden (gele border)
- `.edit-row.disabled` — grayed-out text, dimmed background
- `.edit-row.disabled input[type=text]` — disabled styling
- `.admin-active` — checkbox styling (compact)
- `.validation-error` — rode border + error tooltip

---

## Sectie-indeling (uit globals.csv)

De CSV heeft al section comments. Deze worden 1:1 overgenomen als groepen:

1. AUDIO — intervallen, fading
2. VOLUME — lo/hi, playback, ping, distance
3. TTS CACHE — dir/file index, duration factor
4. SPEECH — saytime, temperature speak intervals
5. LIGHT/PATTERN — change intervals, fade width
6. BRIGHTNESS/LUX — lo/hi, defaults, calibratie gerelateerd
7. SENSORS — distance, lux, timing
8. HEARTBEAT — min/max/default
9. ALERT — flash burst, reminder
10. BOOT/CLOCK — bootstrap, NTP, boot phase
11. NETWORK/FETCH — weather, sun, calendar
12. CSV HTTP — timeout, wait
13. LOCATION — lat/lon
14. TIME FALLBACK — month/day/hour/year
15. DEEP SLEEP — enabled, sleep/wake times
16. TV SIMULATOR — theme box, brightness, audio
17. DEBUG — timer/health status intervals

---

## Beslissingen (2026-04-12)

### A. Scope → ALLE parameters
- Alle uncommented EN commented parameters uit globals.csv
- Commented regels worden getoond als disabled/grayed-out

### B. SD-write → DIRECT met lockSD()
- Directe write vanuit handler, geen deferred flag
- Na deze modal wordt ALTIJD herstart, ook zonder wijzigingen
- In feite mag de rest van het programma worden stilgelegd

### C. Validatie → STRENG
- Types (`u`, `f`, `s`, `b`, `i`) worden NIET getoond aan de gebruiker maar streng gecheckt
- `u` (uint32): alleen niet-negatieve gehele getallen
- `f` (float): geldig decimaal getal
- `s` (string): non-empty
- `b` (bool): alleen 0 of 1
- `i` (int32): geheel getal (mag negatief)
- Logische regels: Lo < Hi paren, intervallen > 0, fracties 0.0-1.0 waar van toepassing
- Validatie in ZOWEL frontend (voorkom foute input) als firmware (reject bij parse)

### D. Commented keys → JA, tonen + auto-uncomment
- Commented regels (met `#` prefix) worden getoond als disabled/grayed-out
- **CRUCIAAL:** wanneer een gebruiker een commented parameter bewerkt (waarde wijzigt),
  wordt de `#` automatisch verwijderd bij save → parameter wordt actief
- Omgekeerd: een "disable" toggle per parameter kan de `#` terugzetten
- Dit is de primaire manier om parameters aan/uit te schakelen

### E. PIN → hergebruik 1951
- Eén PIN voor alle admin functies (`Globals::wifiConfigPin`)

### F. Section headers → bewaren + tonen
- Section headers (`# ═══ SECTION NAME ═══`) worden meegestuurd als JSON markers
- Frontend toont ze als group headers (collapsible)
- Bij SAVE worden section headers 1:1 teruggeschreven in origineel formaat

### G. Save flow → twee knoppen
- **"Save & Restart"**: `confirm("Save settings and restart?")` → POST globals → POST restart
- **"Cancel & Restart"**: geen save, direct POST restart → herstart met ongewijzigde CSV
- ALTIJD restart na modal sluiten (= bewuste keuze)

### I. Modal close-knop → VERWIJDERD
- Geen `✕` knop op de admin modal
- Enige uitwegen: "Save & Restart" of "Cancel & Restart"
- Backdrop-click sluit de modal ook NIET (override in modal.js)
- Voorkomt per-ongeluk sluiten zonder bewuste keuze

### J. PIN foutafhandeling → SIMPEL
- Onbeperkte pogingen
- Geen foutmelding bij verkeerde PIN (stille reject, veld leegmaken)
- PIN is een "weet je zeker?"-drempel, geen echte security

### H. Validatie-definitie → hardcoded in JS
- Lo/Hi paren, fractie-ranges, interval>0 regels: gedefinieerd in `adminSettings.js`
- Simpel, geen extra server-side metadata nodig
- Onderhoud: als er keys bijkomen, JS updaten

---

## Werk-inschatting per component

| # | Component | Files | Afhankelijkheden |
|---|-----------|-------|-----------------|
| 1 | `AdminRoutes.cpp/.h` — GET+POST endpoints | 2 nieuwe | Globals.h, SdFileAccess, AlertState |
| 2 | Route registratie in WebInterfaceController | 1 edit | AdminRoutes.h |
| 3 | `adminSettings.js` — JS module | 1 nieuw | modal.js, main.js |
| 4 | `main.js` — init call toevoegen | 1 edit | adminSettings.js |
| 5 | `index.html` — modal HTML + trigger knop | 1 edit | — |
| 6 | `styles.css` — admin styles | 1 edit | — |
| 7 | `build.ps1` — version bump + nieuw JS file in concat | 1 edit | — |
| 8 | Test & debug | — | Device beschikbaar |

**Totaal:** 2 nieuwe files, 5 edits

---

## Risico's

| Risico | Impact | Mitigatie |
|--------|--------|----------|
| Foute waarde in globals.csv → device boot-loopt | Laag (was Hoog) | Strenge validatie client+server, firmware valt terug op code-defaults bij parse failure |
| SD-write corrupt bij stroomuitval | Middel | Onwaarschijnlijk (write duurt <100ms), acceptabel |
| JSON te groot voor ESP32 heap (~75 items) | Laag | ~75 items × ~80 bytes = ~6KB JSON, ruim binnen heap |
| Modal te vol op mobiel scherm | Middel | Scrollable `.wide` modal + collapsible secties |

---

## Niet in scope

- Bewerken van andere CSV's (light_patterns, light_colors, etc.) — apart project
- Live preview van gewijzigde waarden zonder restart
- Undo/rollback van vorige globals.csv
- Multi-user locking
