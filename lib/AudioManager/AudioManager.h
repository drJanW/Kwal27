/**
 * @file AudioManager.h
 * @brief Main audio playback engine header for ESP32
 * @version 251231E
 * @date 2025-12-31
 *
 * Declares the AudioManager class and AudioOutputI2S_Metered class.
 * AudioManager provides the primary interface for MP3 decoding and I2S audio output.
 * AudioOutputI2S_Metered extends the base I2S output with real-time level metering.
 * Used by PlayFragment, PlaySentence, and PlayPCM modules for audio playback.
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

// Inlined audiometer: geen extra bestand
class AudioOutputI2S_Metered : public AudioOutputI2S {
public:
  using AudioOutputI2S::AudioOutputI2S;

  bool begin() override;
  bool ConsumeSample(int16_t sample[2]) override;

protected:
  void publishLevel();
  friend void cb_audioMeter();
  uint64_t  _acc = 0;
  uint32_t  _cnt = 0;
  bool      _publishDue = false;
};

class AudioManager {
public:
  static AudioManager& instance();

  void begin();
  void stop();
  void update();

  bool startFragment(const AudioFragment& frag);
  void startTTS(const String& phrase);
  struct PCMClipDesc {
    const int16_t* samples = nullptr;
    uint32_t sampleCount = 0;
    uint32_t sampleRate = 0;
    uint32_t durationMs = 0;
  };

  bool playPCMClip(const PCMClipDesc& clip, float amplitude);
  void stopPCMClip();
  bool isPCMClipActive() const;

  void setVolumeWebShift(float value);
  void updateGain();

  AudioOutputI2S_Metered audioOutput;     // was AudioOutputI2S
  AudioFileSource*       audioFile = nullptr;
  AudioGeneratorMP3*     audioMp3Decoder = nullptr;
  HMP3Decoder            helixDecoder = nullptr;

private:
  AudioManager();
  AudioManager(const AudioManager&) = delete;
  AudioManager& operator=(const AudioManager&) = delete;

  void releaseDecoder();
  void releaseSource();
  void finalizePlayback();
  void resetPCMPlayback();
  bool pumpPCMPlayback();

  struct PCMPlayback {
    bool active = false;
    float amplitude = 1.0f;
    uint32_t index = 0;
    uint32_t totalSamples = 0;
    const int16_t* samples = nullptr;
    uint32_t sampleRate = 0;
  } pcmPlayback_;
};
