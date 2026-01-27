/**
 * @file AudioManager.cpp
 * @brief Main audio playback engine for ESP32
 * @version 251231E
 * @date 2025-12-31
 *
 * This file implements the core audio playback functionality for the Kwal project.
 * It manages the MP3 decoder (using libhelix) and I2S output for audio streaming.
 * Includes an integrated audio level meter for real-time volume monitoring.
 * Handles audio output initialization, sample processing, and gain control.
 */

#include "Globals.h"
#include "AudioManager.h"
#include "AudioState.h"
#include "PlayFragment.h"
#include "PlaySentence.h"
#include "TimerManager.h"
#include "MathUtils.h"
#include "Notify/NotifyConduct.h"
#include "Notify/NotifyIntent.h"

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
AudioOutputI2S_Metered* gMeterInstance = nullptr;
constexpr uint32_t kAudioMeterIntervalMs = 50;
constexpr uint16_t kPCMFrameBatch = 96;
} // namespace

void cb_audioMeter();

AudioManager& AudioManager::instance() {
	static AudioManager inst;
	return inst;
}

AudioManager::AudioManager()
	: audioOutput()
{
}

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

bool AudioOutputI2S_Metered::ConsumeSample(int16_t sample[2])
{
	int64_t s = sample[0];
	_acc += s * s;
	_cnt++;
	_publishDue = true;
	return AudioOutputI2S::ConsumeSample(sample);
}

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

void cb_audioMeter()
{
	if (gMeterInstance) {
		gMeterInstance->publishLevel();
	}
}

void AudioManager::releaseDecoder()
{
	if (audioMp3Decoder) {
		audioMp3Decoder->stop();
		delete audioMp3Decoder;
		audioMp3Decoder = nullptr;
	}
}

void AudioManager::releaseSource()
{
	if (audioFile) {
		delete audioFile;
		audioFile = nullptr;
	}
}

void AudioManager::finalizePlayback()
{
	bool wasTts = isTtsActive();
	
	releaseDecoder();
	releaseSource();

	setAudioLevelRaw(0);
	audioOutput.SetGain(getVolumeShiftedHi() * getVolumeWebShift());
	setAudioBusy(false);
	setFragmentPlaying(false);
	setSentencePlaying(false);
	setTtsActive(false);
	
	if (wasTts) {
		NotifyConduct::report(NotifyIntent::TTS_OK);
	}
}

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
	audioOutput.SetGain(getVolumeShiftedHi() * getVolumeWebShift());

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

void AudioManager::stopPCMClip()
{
	resetPCMPlayback();
	if (!audioMp3Decoder) {
		setAudioBusy(false);
		setFragmentPlaying(false);
		setSentencePlaying(false);
		setAudioLevelRaw(0);
		audioOutput.SetGain(getVolumeShiftedHi() * getVolumeWebShift());
	}
}

bool AudioManager::isPCMClipActive() const
{
	return pcmPlayback_.active;
}

void AudioManager::begin()
{
	audioOutput.SetPinout(PIN_I2S_BCLK, PIN_I2S_LRC, PIN_I2S_DOUT);
	audioOutput.begin();
	audioOutput.SetGain(getVolumeShiftedHi() * getVolumeWebShift());
}

void AudioManager::stop()
{
	stopPCMClip();
	finalizePlayback();
}

void AudioManager::update()
{
	if (pcmPlayback_.active && !pumpPCMPlayback()) {
		resetPCMPlayback();
		if (!audioMp3Decoder) {
			setAudioBusy(false);
			setFragmentPlaying(false);
			setSentencePlaying(false);
			setAudioLevelRaw(0);
			audioOutput.SetGain(getVolumeShiftedHi() * getVolumeWebShift());
		}
	}

	if (audioMp3Decoder) {
		audioMp3Decoder->loop();  // Pump data only; completion via cb_fragmentReady/cb_wordTimer
	}
}

bool AudioManager::startFragment(const AudioFragment& frag) {
	if (!PlayAudioFragment::start(frag)) {
		AUDIO_LOG_WARN("[Audio] startFragment failed for %03u/%03u\n", frag.dirIndex, frag.fileIndex);
		return false;
	}
	return true;
}

void AudioManager::startTTS(const String& phrase) {
	PlaySentence::startTTS(phrase);
}

void AudioManager::setVolumeWebShift(float value) {
	::setVolumeWebShift(value);  // no clamp - F9 pattern allows >1.0
	updateGain();
}

void AudioManager::updateGain() {
	PlayAudioFragment::updateGain();
	if (!isFragmentPlaying() && !isSentencePlaying() && !pcmPlayback_.active) {
		audioOutput.SetGain(getVolumeShiftedHi() * getVolumeWebShift());
	}
}

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
