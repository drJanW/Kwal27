# Free Text TTS — Tech Note

> Version: 260411D | Updated: 2026-04-12

Allows the user to type arbitrary Dutch text in the WebGUI, which is synthesized
to speech via VoiceRSS, cached on SD, and optionally repeated at a configurable
interval.

## Data Flow

```
WebGUI (audio.js)
  → POST /api/audio/freetext?text=...&interval=...&dur=...&voice=...&tempo=...&vol=...
    → routeSetFreeTextTts()          [AudioRoutes.cpp — memory only]
      → RunManager::requestSetWebFreeTextTts()
        → stores params in static vars
        → timers.create(1, 1, cb_startWebFreeTextTts)   [deferred — no I/O in handler]
          → cb_startWebFreeTextTts()
            → PlaySentence::downloadTtsToCache()  [HTTP→VoiceRSS, write /127/000.mp3]
            → playFreeTextFromCache()             [AudioPolicy::requestFragment]
            → timers.create(intervalMs, repeatCount, cb_webFreeTextRepeat)
```

## Key Components

| Component | File | Role |
|-----------|------|------|
| Web route | `lib/WebInterfaceController/routes/AudioRoutes.cpp` | Parse params, call RunManager (memory only) |
| Orchestration | `lib/RunManager/RunManager.cpp` (line ~900) | Static state, timer callbacks, download+play |
| TTS download | `lib/AudioManager/PlaySentence.cpp` | `downloadTtsToCache()` — VoiceRSS HTTP, write to SD |
| Playback | `lib/AudioManager/PlayAudioFragment.cpp` | Play from SD via AudioPolicy::requestFragment |
| SSE state | `lib/WebInterfaceController/WebGuiStatus.cpp` | Reports `freeText` field in state event |
| WebGUI JS | `sdroot/webgui-src/js/audio.js` | `initFreeTextControls()`, sends POST, shows UI |
| WebGUI HTML | `sdroot/index.html` | Freetext panel inside audio-settings-modal |

## Parameters

| Param | Route name | Default | Range | Meaning |
|-------|-----------|---------|-------|---------|
| `text` | `text` | — | max 160 chars | Dutch text to speak. Empty = clear. |
| `interval` | `interval` | 5 | 1–720 min | Repeat interval (minutes) |
| `dur` | `dur` | 60 | 1–780 min | Total duration (minutes). repeatCount = dur / interval. |
| `voice` | `voice` | -1 | -1, 0, 1, 2 | -1=random, 0=Lotte, 1=Bram, 2=Daan |
| `tempo` | `tempo` | 99 | -3 to +3 | 99=random. VoiceRSS speed parameter. |
| `vol` | `vol` | 0 | 0–100 | 0=system default, 1–100=override percentage |

## WebGUI Controls

Inside the audio-settings-modal (jukebox section):

| Icon | Element ID | Type | Function |
|------|-----------|------|----------|
| 💬 | `freetext-input` | text input | Dutch text (max 99 chars recommended) |
| 🔊 | `freetext-vol` | slider (green) | Volume: 0=auto, 1–100=% |
| 🗣 | `freetext-voice` | select | Voice: Lotte, Bram, Daan |
| 🏃 | `freetext-tempo` | slider (orange) | Tempo: -3 to +3 |
| 🔁 | `freetext-interval` | slider (purple) | Repeat interval: 1/2/3/5/10/15/30/60 min |
| ▶️ | `freetext-play` | button | Submit text for TTS |
| 🗑️ | `freetext-stop` | button | Clear text and cancel repeat |

## SD Cache

- Directory: `Globals::ttsCacheDirIndex` (127)
- File: `Globals::ttsCacheFileIndex` (0) → `/127/000.mp3`
- Overwritten on each new freetext submission
- Repeat plays from cache (no re-download)

## Timer Pattern

- **Start**: `timers.create(1, 1, cb_startWebFreeTextTts)` — one-shot, 1ms delay (defers I/O from web handler)
- **Repeat**: `timers.create(intervalMs, repeatCount, cb_webFreeTextRepeat)` — N fires then auto-free
- **Clear**: `timers.cancel(cb_startWebFreeTextTts)` + `timers.cancel(cb_webFreeTextRepeat)` + reset all static vars

## Architecture Compliance

- Web handler is **memory only** — no SD, no HTTP, no blocking
- All I/O deferred to timer callback via `timers.create(1, 1, ...)`
- Repeat uses TimerManager `repeat=N` — no self-reschedule
- Audio stops current fragment/sentence before SD write (prevents I/O conflict)
- SSE reports active freetext via `"freeText"` field in state event
