/**
 * @file PlaySentence.cpp
 * @brief TTS sentence playback with word dictionary and VoiceRSS API
 * @version 260219C
 * @date 2026-02-19
 * 
 * Implements sequential word playback from /000/ directory.
 * Uses unified SpeakItem queue for mixing MP3 words and TTS sentences.
 * Timer-driven completion (T4 rule: never use loop() return).
 */
#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClient.h>

#include <AudioFileSourceHTTPStream.h>
#include <AudioFileSourceSD.h>
#include <AudioGeneratorMP3.h>

#include "AudioManager.h"
#include "PlaySentence.h"
#include "Globals.h"
#include "AudioState.h"
#include "WiFiController.h"
#include "MathUtils.h"
#include "TimerManager.h"
#include "SDSettings.h"
#include "Alert/AlertRun.h"
#include "Alert/AlertRequest.h"
#include <SD.h>

extern const char* getMP3Path(uint8_t dirIdx, uint8_t fileIdx);

// --------- Forward declarations to prevent link/scope errors ----------
namespace PlayAudioFragment {
    void stop(uint16_t fadeOutMs = 0xFFFF);
}

// ---------------- Anonymous namespace for internal state and callbacks ---------
namespace {

// Word queue - shift register (kept for playWord compatibility)
uint8_t wordQueue[PlaySentence::MAX_WORDS_PER_SENTENCE];
bool queueInitialized = false;

// === UNIFIED SPEAKITEM QUEUE ===
enum class SpeakItemType : uint8_t { MP3_WORDS, TTS_SENTENCE };

struct SpeakItem {
    SpeakItemType type;
    const void* payload;
};

constexpr uint8_t SPEAK_QUEUE_SIZE = 8;
SpeakItem speakQueue[SPEAK_QUEUE_SIZE];
uint8_t speakQueueHead = 0;
uint8_t speakQueueTail = 0;

// Scratchpad for runtime MP3 arrays (sayTime)
uint8_t mp3Scratchpad[8];

// One-shot flag: next SetGain uses MAX_SPEAK_VOLUME_MULTIPLIER then resets.
// Safe because WELCOME only fires during boot (queue guaranteed empty).
bool forceMax = false;

constexpr uint16_t WORD_FALLBACK_MS = 800;
constexpr uint16_t TTS_CHAR_INTERVAL_MS = 114;   // scaled ~1.2x for r=-2 speech rate
constexpr uint16_t TTS_TAIL_INTERVAL_MS = 2100;   // scaled ~1.2x for r=-2 speech rate

uint16_t countWords(const char* sentence) {
    if (!sentence) return 0;
    uint16_t words = 0;
    bool inWord = false;
    for (size_t i = 0; sentence[i] != '\0'; ++i) {
        char c = sentence[i];
        bool separator = (c <= ' ' || c == '.' || c == ',' || c == ':' || c == ';');
        if (separator) {
            inWord = false;
            continue;
        }
        if (!inWord) {
            words++;
            inWord = true;
        }
    }
    return words;
}

uint32_t calcTtsDurationMs(const char* sentence) {
    uint32_t charMs = strlen(sentence) * TTS_CHAR_INTERVAL_MS + TTS_TAIL_INTERVAL_MS;
    uint16_t words = countWords(sentence);
    uint32_t wordMs = static_cast<uint32_t>(words) * 504U + TTS_TAIL_INTERVAL_MS;  // scaled ~1.2x for r=-2
    return (charMs > wordMs) ? charMs : wordMs;
}

uint16_t wordDurations[SD_MAX_FILES_PER_SUBDIR] = {0};
bool wordDurationsLoaded = false;
uint32_t lastWordsLoadAttemptMs = 0;

void initQueue() {
    if (!queueInitialized) {
        for (uint8_t i = 0; i < PlaySentence::MAX_WORDS_PER_SENTENCE; i++) {
            wordQueue[i] = PlaySentence::END_OF_SENTENCE;
        }
        queueInitialized = true;
    }
}

void shiftQueue() {
    for (uint8_t i = 0; i < PlaySentence::MAX_WORDS_PER_SENTENCE - 1; i++) {
        wordQueue[i] = wordQueue[i + 1];
    }
    wordQueue[PlaySentence::MAX_WORDS_PER_SENTENCE - 1] = PlaySentence::END_OF_SENTENCE;
}

void resetWordDurations() {
    for (uint16_t i = 0; i < SD_MAX_FILES_PER_SUBDIR; ++i) {
        wordDurations[i] = 0;
    }
    wordDurationsLoaded = false;
    lastWordsLoadAttemptMs = 0;
}

bool loadWordDurations() {
    File idx = SD.open(WORDS_INDEX_FILE, FILE_READ);
    if (!idx) {
        PF("[PlaySentence] Missing %s\n", WORDS_INDEX_FILE);
        return false;
    }
    const size_t expectedBytes = static_cast<size_t>(SD_MAX_FILES_PER_SUBDIR) * sizeof(uint16_t);
    size_t readBytes = idx.read(reinterpret_cast<uint8_t*>(wordDurations), expectedBytes);
    idx.close();
    if (readBytes != expectedBytes) {
        PF("[PlaySentence] Corrupt %s (%u/%u bytes)\n",
           WORDS_INDEX_FILE,
           static_cast<unsigned>(readBytes),
           static_cast<unsigned>(expectedBytes));
        resetWordDurations();
        return false;
    }
    wordDurationsLoaded = true;
    return true;
}

uint16_t measureWordDuration(uint8_t mp3Id) {
    char path[20];
    snprintf(path, sizeof(path), "/%03u/%03u.mp3", WORDS_SUBDIR_ID, mp3Id);
    File f = SD.open(path, FILE_READ);
    if (!f) {
        return 0;
    }
    uint32_t sizeBytes = f.size();
    f.close();
    if (sizeBytes == 0) {
        return 0;
    }
    
    // Empirical formula: duration_ms = (size_bytes * 5826) / 100000
    uint32_t durationMs = (sizeBytes * 5826UL) / 100000UL;
    
    if (durationMs > 0xFFFF) {
        durationMs = 0xFFFF;
    }
    return static_cast<uint16_t>(durationMs);
}

uint16_t getMp3DurationMs(uint8_t mp3Id) {
    if (mp3Id >= SD_MAX_FILES_PER_SUBDIR) {
        return WORD_FALLBACK_MS + PlaySentence::WORD_INTERVAL_MS;
    }

    // Always use empirical formula instead of index file
    // (index file has old BYTES_PER_MS/HEADER_MS calculation)
    uint16_t duration = measureWordDuration(mp3Id);

    if (duration == 0) {
        duration = WORD_FALLBACK_MS;
    }

    return duration + PlaySentence::WORD_INTERVAL_MS;
}

String urlencode(const String& s) {
    static const char* hex = "0123456789ABCDEF";
    String out;
    out.reserve(s.length() * 3);
    for (size_t i = 0; i < s.length(); ++i) {
        uint8_t c = static_cast<uint8_t>(s[i]);
        if ((c >= 'A' && c <= 'Z') ||
            (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~') {
            out += static_cast<char>(c);
        } else if (c == ' ') {
            out += '+';
        } else {
            out += '%';
            out += hex[(c >> 4) & 0x0F];
            out += hex[c & 0x0F];
        }
    }
    return out;
}

// Timer callback - MUST be in anonymous namespace (function pointer for TimerManager)
cb_type cb_wordTimer() {
    PlaySentence::playWord();
}

// Forward declaration for unified queue
void playNextSpeakItem();

// TTS completion callback (T4: timer-based, not loop() return)
void cb_ttsReady() {
    // Cleanup decoder
    if (audio.audioMp3Decoder) {
        audio.audioMp3Decoder->stop();
        delete audio.audioMp3Decoder;
        audio.audioMp3Decoder = nullptr;
    }
    if (audio.audioFile) {
        delete audio.audioFile;
        audio.audioFile = nullptr;
    }
    setTtsActive(false);
    setSentencePlaying(false);
    setAudioBusy(false);
    AlertRun::report(AlertRequest::TTS_OK);
    // Continue with next item in queue
    playNextSpeakItem();
}

void enqueue(SpeakItemType type, const void* payload) {
    if (((speakQueueTail + 1) % SPEAK_QUEUE_SIZE) == speakQueueHead) {
        PL("[PlaySentence] Queue full, dropping");
        return;
    }
    // TTS strings: store owned copy (caller's String may destruct)
    const void* stored = payload;
    if (type == SpeakItemType::TTS_SENTENCE) {
        stored = strdup(static_cast<const char*>(payload));
        if (!stored) {
            PL("[PlaySentence] strdup failed, dropping");
            return;
        }
    }
    speakQueue[speakQueueTail] = { type, stored };
    speakQueueTail = (speakQueueTail + 1) % SPEAK_QUEUE_SIZE;
}

// === TTS Helper functions (within anonymous namespace) ===
struct TtsVoice { const char* lang; const char* name; };
constexpr TtsVoice ttsVoices[] = {
    { "nl-nl", "Daan" },   // Netherlands male
    { "nl-nl", "Lotte" },  // Netherlands female
    { "nl-nl", "Bram" },   // Netherlands male
    { "nl-be", "Daan" },   // Flemish male
    { "nl-be", "Lotte" },  // Flemish female
    { "nl-be", "Bram" },   // Flemish male
};
constexpr uint8_t TTS_VOICE_COUNT = sizeof(ttsVoices) / sizeof(ttsVoices[0]);

String makeVoiceRSSUrl(const String& text) {
    const TtsVoice& v = ttsVoices[random(0, TTS_VOICE_COUNT)];
    PF("[PlaySentence] TTS voice: %s / %s\n", v.lang, v.name);
    return String("http://api.voicerss.org/?key=") + VOICERSS_API_KEY +
           "&hl=" + v.lang + "&v=" + v.name +
           "&r=-2&c=MP3&f=44khz_16bit_mono&src=" + urlencode(text);
}

bool voicerss_ok(const String& url, String& err) {
    HTTPClient http;
    WiFiClient client;

    http.setTimeout(5000);
    if (!http.begin(client, url)) {
        err = "init failed";
        return false;
    }

    const char* keys[] = {"Content-Type"};
    http.collectHeaders(keys, 1);
    http.addHeader("Range", "bytes=0-255");
    int code = http.GET();
    String ct = http.header("Content-Type");

    // Read response head if immediately available (no polling)
    String head;
    WiFiClient* s = http.getStreamPtr();
    if (s) {
        while (s->available() && head.length() < 256) {
            head += char(s->read());
        }
    }
    http.end();

    if ((code == 200 || code == 206) && ct.startsWith("audio/")) {
        return true;
    }
    if (head.startsWith("ERROR")) {
        err = head;
        return false;
    }
    err = String("HTTP ") + code + " CT:" + ct;
    return false;
}

// Internal TTS start (called by playNextSpeakItem)
void startTTSInternal(const char* text) {
    // Stop any current audio first
    if (audio.audioMp3Decoder) {
        audio.audioMp3Decoder->stop();
        delete audio.audioMp3Decoder;
        audio.audioMp3Decoder = nullptr;
    }
    if (audio.audioFile) {
        delete audio.audioFile;
        audio.audioFile = nullptr;
    }

    setAudioBusy(true);
    setSentencePlaying(true);
    setTtsActive(true);
    setWordPlaying(false);
    setCurrentWordId(PlaySentence::END_OF_SENTENCE);

    float speakVolumeMultiplier = forceMax ? MAX_SPEAK_VOLUME_MULTIPLIER : MathUtils::clamp(getVolumeShiftedHi() * 1.8f, 0.0f, 1.0f);
    forceMax = false;
    audio.audioOutput.SetGain(speakVolumeMultiplier);

    String url = makeVoiceRSSUrl(text);
    {
        String apiErr;
        if (!voicerss_ok(url, apiErr)) {
            PF("[TTS] VoiceRSS error: %s\n", apiErr.c_str());
            setAudioBusy(false);
            setSentencePlaying(false);
            setTtsActive(false);
            AlertRun::report(AlertRequest::TTS_FAIL);
            return;
        }
    }

    audio.audioFile = new AudioFileSourceHTTPStream(url.c_str());
    audio.audioMp3Decoder = new AudioGeneratorMP3();
    audio.audioMp3Decoder->begin(audio.audioFile, &audio.audioOutput);
}

void playNextSpeakItem() {
    // Queue empty?
    if (speakQueueHead == speakQueueTail) {
        setAudioBusy(false);
        setSentencePlaying(false);
        return;
    }
    
    // Stop fragment if playing
    if (isFragmentPlaying()) {
        PlayAudioFragment::stop();
    }
    
    SpeakItem& item = speakQueue[speakQueueHead];
    speakQueueHead = (speakQueueHead + 1) % SPEAK_QUEUE_SIZE;
    
    uint32_t durationMs = 0;
    
    if (item.type == SpeakItemType::TTS_SENTENCE) {
        const char* sentence = static_cast<const char*>(item.payload);
        
        startTTSInternal(sentence);
        durationMs = calcTtsDurationMs(sentence);
        PF("[TTS] %s (%ums)\n", sentence, durationMs);
        free(const_cast<void*>(item.payload));  // release strdup'd copy
        
        // T4: Timer-based completion
        timers.cancel(cb_ttsReady);
        timers.create(durationMs, 1, cb_ttsReady);
        
    } else {  // MP3_WORDS
        const uint8_t* words = static_cast<const uint8_t*>(item.payload);
        
        // Copy to wordQueue for playWord() logic
        initQueue();
        uint8_t i = 0;
        while (words[i] != PlaySentence::END_OF_SENTENCE && i < PlaySentence::MAX_WORDS_PER_SENTENCE - 1) {
            wordQueue[i] = words[i];
            i++;
        }
        wordQueue[i] = PlaySentence::END_OF_SENTENCE;
        
        // Start first word
        PlaySentence::playWord();
        
        // Calculate total duration
        for (uint8_t j = 0; j < i; j++) {
            durationMs += getMp3DurationMs(wordQueue[j]);
        }
        PF("[MP3] Started %u words (%ums)\n", i, durationMs);
    }
}

} // anonymous namespace

// ============================================================================
// PlaySentence namespace - public API
// ============================================================================
namespace PlaySentence {

void playWord() {
    initQueue();
    
    if (wordQueue[0] == END_OF_SENTENCE) {
        setSentencePlaying(false);
        setWordPlaying(false);
        setAudioBusy(false);
        setCurrentWordId(END_OF_SENTENCE);
        PL("[PlaySentence] Queue empty, done");
        playNextSpeakItem();
        return;
    }
    
    uint8_t mp3Id = wordQueue[0];
    setCurrentWordId(mp3Id);
    
    const char* path = getMP3Path(0, mp3Id);
    PF("[PlaySentence] Attempting word %u from %s\n", mp3Id, path);
    
    // Cleanup previous
    if (audio.audioMp3Decoder) {
        audio.audioMp3Decoder->stop();
        delete audio.audioMp3Decoder;
        audio.audioMp3Decoder = nullptr;
    }
    if (audio.audioFile) {
        delete audio.audioFile;
        audio.audioFile = nullptr;
    }
    
    audio.audioOutput.SetGain(forceMax ? MAX_SPEAK_VOLUME_MULTIPLIER : MathUtils::clamp(getVolumeShiftedHi() * 1.5f, 0.0f, 1.0f));
    forceMax = false;
    
    auto* sdFile = new AudioFileSourceSD(path);
    if (!sdFile->isOpen()) {
        delete sdFile;
        PF("[PlaySentence] ERROR: Cannot open %s - skipping word\n", path);
        // Skip this word and continue with next
        shiftQueue();
        if (wordQueue[0] != END_OF_SENTENCE) {
            timers.restart(50, 1, cb_wordTimer);  // Try next word quickly
        } else {
            // Sentence done (all words skipped or finished)
            setSentencePlaying(false);
            setAudioBusy(false);
            playNextSpeakItem();
        }
        return;
    }
    audio.audioFile = sdFile;
    
    audio.audioMp3Decoder = new AudioGeneratorMP3();
    if (!audio.audioMp3Decoder->begin(audio.audioFile, &audio.audioOutput)) {
        delete audio.audioFile;
        audio.audioFile = nullptr;
        delete audio.audioMp3Decoder;
        audio.audioMp3Decoder = nullptr;
        PF("[PlaySentence] ERROR: Decoder failed for %s - skipping word\n", path);
        // Skip this word and continue with next
        shiftQueue();
        if (wordQueue[0] != END_OF_SENTENCE) {
            timers.restart(50, 1, cb_wordTimer);  // Try next word quickly
        } else {
            // Sentence done (all words skipped or finished)
            setSentencePlaying(false);
            setAudioBusy(false);
            playNextSpeakItem();
        }
        return;
    }
    
    // Success - remove word from queue and set playback flags
    shiftQueue();
    setWordPlaying(true);
    setSentencePlaying(true);
    setAudioBusy(true);
    
    PF("[PlaySentence] Playing word %u\n", mp3Id);
    
    uint16_t durationMs = getMp3DurationMs(mp3Id);
    timers.restart(durationMs, 1, cb_wordTimer);
}

void addWords(const uint8_t* words) {
    bool wasEmpty = (speakQueueHead == speakQueueTail);
    enqueue(SpeakItemType::MP3_WORDS, words);
    PF("[PlaySentence] Queued MP3 words\n");
    
    if (wasEmpty && !isAudioBusy()) {
        playNextSpeakItem();
    }
}

void addTTS(const char* sentence) {
    bool wasEmpty = (speakQueueHead == speakQueueTail);
    enqueue(SpeakItemType::TTS_SENTENCE, sentence);
    
    if (wasEmpty && !isAudioBusy()) {
        playNextSpeakItem();
    }
}

uint8_t* getScratchpad() {
    return mp3Scratchpad;
}

void stop() {
    timers.cancel(cb_ttsReady);
    timers.cancel(cb_wordTimer);
    
    // Free any remaining strdup'd TTS strings before clearing queue
    while (speakQueueHead != speakQueueTail) {
        SpeakItem& item = speakQueue[speakQueueHead];
        if (item.type == SpeakItemType::TTS_SENTENCE && item.payload) {
            free(const_cast<void*>(item.payload));
        }
        speakQueueHead = (speakQueueHead + 1) % SPEAK_QUEUE_SIZE;
    }
    speakQueueHead = 0;
    speakQueueTail = 0;
    
    // Clear word queue
    for (uint8_t i = 0; i < MAX_WORDS_PER_SENTENCE; i++) {
        wordQueue[i] = END_OF_SENTENCE;
    }
    
    if (audio.audioMp3Decoder) {
        audio.audioMp3Decoder->stop();
        delete audio.audioMp3Decoder;
        audio.audioMp3Decoder = nullptr;
    }
    if (audio.audioFile) {
        delete audio.audioFile;
        audio.audioFile = nullptr;
    }
    setSentencePlaying(false);
    setAudioBusy(false);
    setTtsActive(false);
    setWordPlaying(false);
    setCurrentWordId(END_OF_SENTENCE);
}

void update() {
    // Housekeeping only - queue updates via speakNext
}

void speakNext() {
    // Called by AudioManager when decoder stops
    if (speakQueueHead != speakQueueTail) {
        playNextSpeakItem();
    }
}

void forceMaxVolume() {
    forceMax = true;
}

// Legacy API - for backwards compatibility
void startTTS(const String& text) {
    addTTS(text.c_str());
}

} // namespace PlaySentence
