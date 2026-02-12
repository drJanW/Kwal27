/**
 * @file AudioManager.h
 * @brief Main audio playback coordinator for ESP32 I2S output
 * @version 260212C
 * @date 2026-02-12
 * 
 * AudioManager coordinates all audio output: MP3 fragments, TTS sentences,
 * and PCM clips (ping sounds). It owns the I2S hardware and shared decoder
 * resources. Actual playback logic is delegated to PlayFragment and PlaySentence.
 * 
 * Key responsibilities:
 * - Initialize I2S output and volume settings
 * - Route update() calls to active playback module
 * - Manage shared resources (audioFile, decoder, helix)
 * - Prevent concurrent audio via status flags
 * - Handle PCM clip playback for distance sensor feedback
 */
#pragma once

#include <Arduino.h>
#include <AudioOutputI2S.h>
#include <AudioFileSource.h>
#include "AudioFileSourceSD.h"
#include "AudioGeneratorMP3.h"
#include "libhelix-mp3/mp3dec.h"
#include "Globals.h"

struct AudioFragment;

/**
 * @brief I2S output with audio level metering
 * 
 * Extends AudioOutputI2S to accumulate sample energy for VU meter display.
 * Timer callback cb_audioMeter() triggers periodic level publishing.
 */
class AudioOutputI2S_Metered : public AudioOutputI2S {
public:
  using AudioOutputI2S::AudioOutputI2S;

  bool begin() override;
  bool ConsumeSample(int16_t sample[2]) override;

protected:
  void publishLevel();              ///< Compute RMS and publish to status
  friend void cb_audioMeter();      ///< Timer callback to trigger publish
  uint64_t  _acc = 0;               ///< Accumulated sample energy (sum of squares)
  uint32_t  _cnt = 0;               ///< Sample count since last publish
  bool      _publishDue = false;    ///< Flag set by timer, cleared after publish
};

/**
 * @brief Central audio playback coordinator
 * 
 * Single global instance `audio` manages all audio output. Never plays
 * multiple sources simultaneously - fragments, sentences, and PCM clips
 * are mutually exclusive.
 */
class AudioManager {
public:
  AudioManager();

  /// Initialize I2S output and base volume (call once at boot)
  void begin();
  
  /// Stop all active playback and release resources
  void stop();
  
  /// Pump active playback (call from loop via timer)
  void update();

  /// Start MP3 fragment playback with fade support
  /// @param frag Fragment descriptor (dir, file, start, duration, fade)
  /// @return true if playback started successfully
  bool startFragment(const AudioFragment& frag);
  
  /// Start TTS phrase playback (currently unused, placeholder)
  void startTTS(const String& phrase);
  
  /**
   * @brief Descriptor for raw PCM audio clip
   * Used for ping sounds and other short effects stored in PROGMEM
   */
  struct PCMClipDesc {
    const int16_t* samples = nullptr;   ///< Pointer to sample data
    uint32_t sampleCount = 0;           ///< Total samples in clip
    uint32_t sampleRate = 0;            ///< Sample rate (e.g., 22050)
    uint32_t durationMs = 0;            ///< Precomputed duration in ms
  };

  /// Play a PCM clip at specified amplitude (0.0-1.0)
  bool playPCMClip(const PCMClipDesc& clip, float amplitude);
  
  /// Stop PCM clip playback immediately
  void stopPCMClip();
  
  /// Check if PCM clip is currently playing
  bool isPCMClipActive() const;

  /// Set web UI volume multiplier
  void setVolumeWebMultiplier(float value);
  
  /// Recalculate and apply volume from all volume sources
  void updateVolume();

  // Shared audio resources (public for PlayFragment/PlaySentence access)
  AudioOutputI2S_Metered audioOutput;       ///< I2S output with metering
  AudioFileSource*       audioFile = nullptr;       ///< Current MP3 file source
  AudioGeneratorMP3*     audioMp3Decoder = nullptr; ///< ESP8266Audio MP3 decoder
  HMP3Decoder            helixDecoder = nullptr;    ///< Helix decoder for seeking

  // Non-copyable singleton
  AudioManager(const AudioManager&) = delete;
  AudioManager& operator=(const AudioManager&) = delete;

private:
  void releaseDecoder();      ///< Free MP3 decoder resources
  void releaseSource();       ///< Close and free file source
  void finalizePlayback();    ///< Clean up after playback completes
  void resetPCMPlayback();    ///< Reset PCM state machine
  bool pumpPCMPlayback();     ///< Feed PCM samples to I2S output

  /// PCM playback state machine
  struct PCMPlayback {
    bool active = false;              ///< Playback in progress
    float amplitude = 1.0f;           ///< Volume multiplier
    uint32_t index = 0;               ///< Current sample index
    uint32_t totalSamples = 0;        ///< Total samples to play
    const int16_t* samples = nullptr; ///< Pointer to sample data
    uint32_t sampleRate = 0;          ///< Sample rate for timing
  } pcmPlayback_;
};

/// Global audio manager instance
extern AudioManager audio;
