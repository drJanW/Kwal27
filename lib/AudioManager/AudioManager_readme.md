# AudioManager — Audio Playback Module

**Role in Kwal27:** Owns all audio output hardware (I2S) and coordinates playback of MP3 fragments, TTS sentences, and PCM sound effects. No concurrent playback is allowed — fragments, sentences, and clips are mutually exclusive.

## Files

### AudioManager.h / AudioManager.cpp
Central audio coordinator. Owns the I2S output (`AudioOutputI2S_Metered` — extended with VU metering), the shared MP3 decoder (libhelix), and a single `AudioFileSourceSD`. Manages `AudioStatus` flags to prevent overlapping playback. Routes `update()` to whichever sub-player is active.

- `AudioOutputI2S_Metered` — I2S output class that accumulates sample energy and publishes RMS levels for the web GUI VU meter.
- `AudioManager` — Singleton coordinator (`extern AudioManager audio`). API: `begin()`, `update()`, `stop()`, `startFragment()`, `startTTS()`, plus PCM clip helpers via `PCMClipDesc`.

### PlayFragment.h / PlayFragment.cpp
MP3 fragment playback engine. Loads a file from `"/DDD/FFF.mp3"`, seeks to `startMs`, plays for `durationMs`, and applies a sine-power fade-in/fade-out curve driven by timer callbacks. The `AudioFragment` struct carries all parameters (dir index, file index, score, start, duration, fade, volume override, source label).

### PlayPCM.h / PlayPCM.cpp
Raw 16-bit mono PCM WAV loader and player for short sound effects. Used for the distance-sensor ping (`/ping.wav`) and alert sounds. Enforced format: 22050 Hz, mono, 16-bit. API: `loadFromSD()` caches a WAV into RAM; `play()` starts it at a given volume.

### PlaySentence.h / PlaySentence.cpp
TTS sentence sequencer. Plays sequences of pre-recorded word MP3s from `"/000/"` directory, separated by `WORD_INTERVAL_MS` (150 ms). Words are dispatched from a unified speak queue (`SpeakItem`) that also supports VoiceRSS TTS sentences. API: `addWords()`, `addTTS()`, `playWord()` (callback-driven), `getScratchpad()` for composing word arrays at runtime.