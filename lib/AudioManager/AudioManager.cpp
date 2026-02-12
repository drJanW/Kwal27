/**
 * @file AudioManager.cpp
 * @brief Main audio playback coordinator for ESP32 I2S output
 * @version 260212C
 * @date 2026-02-12
 * 
 * Implements AudioManager and AudioOutputI2S_Metered classes.
 * Handles I2S initialization, PCM clip playback, and resource management.
 * MP3 fragment and sentence playback are delegated to PlayFragment/PlaySentence.
 */
#include "Globals.h"
#include "AudioManager.h"
#include "AudioState.h"
#include "PlayFragment.h"
#include "PlaySentence.h"
#include "TimerManager.h"
#include "MathUtils.h"
#include "Alert/AlertRun.h"
#include "Alert/AlertRequest.h"

#ifndef LOG_AUDIO_VERBOSE
#define LOG_AUDIO_VERBOSE 0
#endif

#if LOG_AUDIO_VERBOSE
#define AUDIO_LOG_INFO(...)  LOG_INFO(__VA_ARGS__)
#define AUDIO_LOG_DEBUG(...) LOG_DEBUG(__VA_ARGS__)
#else
#define AUDIO_LOG_INFO(...)
#define AUDIO_LOG_DEBUG(...)
#endif

#define AUDIO_LOG_WARN(...)  LOG_WARN(__VA_ARGS__)
#define AUDIO_LOG_ERROR(...) LOG_ERROR(__VA_ARGS__)

namespace {
/// Global pointer for timer callback access to metered output instance
AudioOutputI2S_Metered* gMeterInstance = nullptr;

/// VU meter update interval (50ms = 20 updates/sec)
constexpr uint32_t kAudioMeterIntervalMs = 50;

/// PCM samples to pump per update() call
constexpr uint16_t kPCMFrameBatch = 96;
} // namespace

/// Timer callback for audio level metering
void cb_audioMeter();

/// Global audio manager instance
AudioManager audio;

//─────────────────────────────────────────────────────────────────────────────
// AudioManager construction
//─────────────────────────────────────────────────────────────────────────────

AudioManager::AudioManager()
	: audioOutput()
{
}

//─────────────────────────────────────────────────────────────────────────────
// AudioOutputI2S_Metered - I2S output with VU meter support
//─────────────────────────────────────────────────────────────────────────────

/// Initialize metered output: reset accumulators and start meter timer
bool AudioOutputI2S_Metered::begin()
{
	_acc = 0;
	_cnt = 0;
	_publishDue = false;
	setAudioLevelRaw(0);

	gMeterInstance = this;

	timers.cancel(cb_audioMeter);
	if (!timers.create(kAudioMeterIntervalMs, 0, cb_audioMeter)) {
		AUDIO_LOG_ERROR("[AudioMeter] Failed to start meter timer\n");
	}

	return AudioOutputI2S::begin();
}

/// Accumulate sample energy for RMS calculation
bool AudioOutputI2S_Metered::ConsumeSample(int16_t sample[2])
{
	int64_t s = sample[0];
	_acc += s * s;
	_cnt++;
	_publishDue = true;
	return AudioOutputI2S::ConsumeSample(sample);
}

/// Compute RMS level and publish to audio state
void AudioOutputI2S_Metered::publishLevel()
{
	if (!_publishDue || _cnt == 0) {
		return;
	}

	_publishDue = false;
	float mean = static_cast<float>(_acc) / static_cast<float>(_cnt);
	uint16_t rms = static_cast<uint16_t>(sqrtf(mean));
	setAudioLevelRaw(rms);
	_acc = 0;
	_cnt = 0;
}

/// Timer callback: trigger level publish on meter instance
void cb_audioMeter()
{
	if (gMeterInstance) {
		gMeterInstance->publishLevel();
	}
}

//─────────────────────────────────────────────────────────────────────────────
// Resource management
//─────────────────────────────────────────────────────────────────────────────

/// Stop and release MP3 decoder
void AudioManager::releaseDecoder()
{
	if (audioMp3Decoder) {
		audioMp3Decoder->stop();
		delete audioMp3Decoder;
		audioMp3Decoder = nullptr;
	}
}

/// Close and release audio file source
void AudioManager::releaseSource()
{
	if (audioFile) {
		delete audioFile;
		audioFile = nullptr;
	}
}

/// Clean up after any playback completes: release resources, reset state flags
void AudioManager::finalizePlayback()
{
	bool wasTts = isTtsActive();
	
	releaseDecoder();
	releaseSource();

	setAudioLevelRaw(0);
	audioOutput.SetGain(getVolumeShiftedHi() * getVolumeWebMultiplier());
	setAudioBusy(false);
	setFragmentPlaying(false);
	setSentencePlaying(false);
	setTtsActive(false);
	
	if (wasTts) {
		AlertRun::report(AlertRequest::TTS_OK);
	}
}

//─────────────────────────────────────────────────────────────────────────────
// PCM clip playback (ping sounds, alerts)
//─────────────────────────────────────────────────────────────────────────────

/// Start PCM clip playback, stopping any active audio first
bool AudioManager::playPCMClip(const PCMClipDesc& clip, float amplitude)
{
	stopPCMClip();

	if (isFragmentPlaying()) {
		PlayAudioFragment::abortImmediate();
	}
	if (isSentencePlaying()) {
		PlaySentence::stop();
	}

	finalizePlayback();

	if (!clip.samples || clip.sampleCount == 0 || clip.sampleRate == 0) {
		AUDIO_LOG_ERROR("[Audio] playPCMClip: invalid clip\n");
		return false;
	}

	amplitude = MathUtils::clamp01(amplitude);

	pcmPlayback_.active = true;
	pcmPlayback_.amplitude = amplitude;
	pcmPlayback_.index = 0;
	pcmPlayback_.totalSamples = clip.sampleCount;
	pcmPlayback_.samples = clip.samples;
	pcmPlayback_.sampleRate = clip.sampleRate;

	audioOutput.SetRate(static_cast<int>(clip.sampleRate));
	audioOutput.SetBitsPerSample(16);
	audioOutput.SetChannels(2);
	audioOutput.begin();
	audioOutput.SetGain(getVolumeShiftedHi() * getVolumeWebMultiplier());

	AUDIO_LOG_DEBUG("[Audio] PCM playback start: samples=%lu sr=%lu amp=%.2f\n",
		static_cast<unsigned long>(pcmPlayback_.totalSamples),
		static_cast<unsigned long>(clip.sampleRate),
		static_cast<double>(pcmPlayback_.amplitude));

	setAudioBusy(true);
	setFragmentPlaying(false);
	setSentencePlaying(false);
	setAudioLevelRaw(0);
	return true;
}

/// Stop PCM playback and reset state if no MP3 decoder active
void AudioManager::stopPCMClip()
{
	resetPCMPlayback();
	if (!audioMp3Decoder) {
		setAudioBusy(false);
		setFragmentPlaying(false);
		setSentencePlaying(false);
		setAudioLevelRaw(0);
		audioOutput.SetGain(getVolumeShiftedHi() * getVolumeWebMultiplier());
	}
}

/// Check if PCM clip is currently playing
bool AudioManager::isPCMClipActive() const
{
	return pcmPlayback_.active;
}

//─────────────────────────────────────────────────────────────────────────────
// Public API
//─────────────────────────────────────────────────────────────────────────────

/// Initialize I2S output with configured pins and default volume
void AudioManager::begin()
{
	audioOutput.SetPinout(PIN_I2S_BCLK, PIN_I2S_LRC, PIN_I2S_DOUT);
	audioOutput.begin();
	audioOutput.SetGain(getVolumeShiftedHi() * getVolumeWebMultiplier());
}

/// Stop all active audio playback
void AudioManager::stop()
{
	stopPCMClip();
	finalizePlayback();
}

/// Main update loop: pump PCM samples and MP3 decoder
void AudioManager::update()
{
	if (pcmPlayback_.active && !pumpPCMPlayback()) {
		resetPCMPlayback();
		if (!audioMp3Decoder) {
			setAudioBusy(false);
			setFragmentPlaying(false);
			setSentencePlaying(false);
			setAudioLevelRaw(0);
			audioOutput.SetGain(getVolumeShiftedHi() * getVolumeWebMultiplier());
		}
	}

	if (audioMp3Decoder) {
		audioMp3Decoder->loop();  // Pump data only; completion via cb_fragmentReady/cb_wordTimer
	}
}

/// Start MP3 fragment playback (delegates to PlayFragment)
bool AudioManager::startFragment(const AudioFragment& frag) {
	if (!PlayAudioFragment::start(frag)) {
		AUDIO_LOG_WARN("[Audio] startFragment failed for %03u/%03u\n", frag.dirIndex, frag.fileIndex);
		return false;
	}
	return true;
}

/// Start TTS phrase playback (delegates to PlaySentence)
void AudioManager::startTTS(const String& phrase) {
	PlaySentence::startTTS(phrase);
}

/// Set web UI volume multiplier and recalculate volume
void AudioManager::setVolumeWebMultiplier(float value) {
	::setVolumeWebMultiplier(value);  // no clamp - F9 pattern allows >1.0
	updateVolume();
}

/// Recalculate and apply volume from all volume sources
void AudioManager::updateVolume() {
	PlayAudioFragment::updateVolume();
	if (!isFragmentPlaying() && !isSentencePlaying() && !pcmPlayback_.active) {
		audioOutput.SetGain(getVolumeShiftedHi() * getVolumeWebMultiplier());
	}
}

//─────────────────────────────────────────────────────────────────────────────
// PCM playback internals
//─────────────────────────────────────────────────────────────────────────────

/// Reset PCM playback state machine after clip completes
void AudioManager::resetPCMPlayback()
{
	if (!pcmPlayback_.active) {
		return;
	}

	AUDIO_LOG_DEBUG("[Audio] PCM playback finished (samples=%lu)\n",
		static_cast<unsigned long>(pcmPlayback_.totalSamples));

	pcmPlayback_.active = false;
	pcmPlayback_.index = 0;
	pcmPlayback_.totalSamples = 0;
	pcmPlayback_.samples = nullptr;
	pcmPlayback_.sampleRate = 0;

	audioOutput.flush();
	audioOutput.stop();
}

/// Feed PCM samples to I2S output in batches
/// @return true if more samples remain, false when clip complete
bool AudioManager::pumpPCMPlayback()
{
	if (!pcmPlayback_.active || !pcmPlayback_.samples) {
		return false;
	}

	uint16_t produced = 0;
	int16_t frame[2];
	const int16_t* samples = pcmPlayback_.samples;

	while (produced < kPCMFrameBatch && pcmPlayback_.index < pcmPlayback_.totalSamples) {
		const int16_t rawSample = samples[pcmPlayback_.index];
		float scaled = static_cast<float>(rawSample) * pcmPlayback_.amplitude;
		if (scaled > 32767.0f) scaled = 32767.0f;
		if (scaled < -32768.0f) scaled = -32768.0f;
		const int16_t value = static_cast<int16_t>(scaled);
		frame[0] = value;
		frame[1] = value;

		if (!audioOutput.ConsumeSample(frame)) {
			break;
		}
		++pcmPlayback_.index;
		++produced;
	}

	audioOutput.loop();

	return pcmPlayback_.index < pcmPlayback_.totalSamples;
}
