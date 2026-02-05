# Audio Subsystem

> Version: 260205D | Updated: 2026-02-05

This module describes the responsibilities and collaboration between AudioManager, PlayFragment, and PlaySentence. The design is optimized for reliable, non-blocking MP3 playback on ESP32, with fade support for fragments and sequential playback of individual words using a fixed word dictionary.

1. AudioManager – Playback Coordinator and Resource Controller
AudioManager is responsible for:

Initializing the I2S output and base gain (begin())

Calling PlayFragment::start() or PlaySentence::start()

Forwarding update() to the appropriate module:

PlayFragment::update() if isFragmentPlaying()

PlaySentence::update() if isSentencePlaying()

Managing shared audio objects:

audioFile

audioMp3Decoder

audioOutput

helixDecoder

Preventing concurrent audio playback via global status flags:

setFragmentPlaying(bool)

setSentencePlaying(bool)

Providing a safe teardown mechanism through stop()

What AudioManager no longer does:

No fading logic

No gain timing or adjustment

No decoder looping

No playback duration tracking

All actual playback logic is fully delegated to PlayFragment or PlaySentence.

2. PlayFragment – Audio Fragment Playback Module
PlayFragment handles playback of MP3 fragments from arbitrary SD card subdirectories. Each fragment is played from a specified start time and for a given duration, optionally with fade effects.

Responsibilities:

Build file path via getMP3Path(dir, file)

Set up audioFile, audioMp3Decoder, and helixDecoder

Execute seekToValidFrame() using Helix decoder

Launch three timers:

Fade-in timer at t = 0

Fade-out timer at t = durationMs - fadeMs

End-of-playback timer at t = durationMs

Start a fourth repeating timer which periodically:

Calculates getFFade() × getBaseGain()

Calls SetGain(...) accordingly

Fading and gain logic are completely decoupled from playback status. Playback ends automatically when the duration timer expires. No use of loop() or isRunning().

3. PlaySentence – Sequential Word Playback Module
PlaySentence plays a sequence of fixed MP3 word IDs from directory /000/. Words are played one at a time using a simple shifting mechanism.

Features:

Internally uses a static array sentence[MAX_WORDS_PER_SENTENCE]

Words are shifted left when one finishes

END_OF_SENTENCE marks the end of the sequence

Each word is played via the internal function PlayWord(wordID)

PlayWord():

Builds the MP3 path

Starts the file

Estimates duration as (fileSize / 16) - HEADER_MS

Starts a timer that sets setSentencePlaying(false) when done

No use of decoder loop() or state checks

Only one sentence is active at a time

All word playback is strictly time-driven. A new word only starts after the previous one finishes.

4. Shared Design Principles
All MP3s are mono, 128 kbps

Playback duration is calculated as durationMs = (fileSize / 16) - HEADER_MS

Playback state is managed exclusively via muxed flags in Globals:

isFragmentPlaying()

isSentencePlaying()

isAudioPlaying() = derived getter

Fading is controlled externally using fFade and fadeArray[]

Gain is applied as SetGain(fFade × baseGain)

5. Files
AudioManager.h/.cpp: central coordinator and resource controller

PlayFragment.h/.cpp: non-blocking fragment playback with fade support

PlaySentence.h/.cpp: sequential word playback using fixed MP3 word IDs

~~PlayStream.cpp/h~~: **Removed** (Dec 2025) - was unused dead code for URL streaming

This structure is designed for robustness, non-blocking operation, and precise playback timing — without reliance on decoder state or blocking loops.

## Audio Timing

Fragment playback uses a **random interval** between 6-18 minutes (configurable via `MIN_AUDIO_INTERVAL_MS` / `MAX_AUDIO_INTERVAL_MS` in `globals.h`). This replaces the previous fixed 10-minute interval to create more natural, less predictable audio behavior.

6. PCM Clip Policy and Integration 
- Any clip is distributed as `/?????.wav` and must remain a 44-byte header PCM WAV: RIFF, WAVE, `fmt ` chunk, 16-bit little-endian, mono, 22 050 Hz.
- No ancillary chunks or metadata are permitted; the file is header + sample payload only.
- `PlayPCM::loadFromSD()` validates the header, loads the payload into RAM, and returns an `AudioManager::PCMClipDesc` plus the owning `std::unique_ptr`.
- Run code registers the loaded clip through `setDistanceClipPointer()` so the data stays under Run ownership.
- `AudioManager::playPCMClip()` now consumes that `PCMClipDesc` directly without any intermediate helper classes; all PCM streaming lives inside `AudioManager`.
- The loader verifies only the invariants above; anything else is considered malformed.