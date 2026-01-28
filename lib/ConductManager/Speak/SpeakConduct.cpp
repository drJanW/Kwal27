/**
 * @file SpeakConduct.cpp
 * @brief TTS speech state management implementation
 * @version 251231E
 * @date 2025-12-31
 *
 * Implements speech routing: maps SpeakIntent values to word sequences
 * (component names for failures, numbers for time), and queues them via
 * PlaySentence for sequential MP3 playback.
 */

#include "SpeakConduct.h"
#include "SpeakPolicy.h"
#include "SpeakWords.h"
#include "PlaySentence.h"
#include "Audio/AudioPolicy.h"
#include "Notify/NotifyState.h"
#include "WiFiManager.h"
#include "PRTClock.h"
#include "SDManager.h"
#include "Globals.h"

namespace {

// TTS zinnen (primair)
const char* getTtsSentence(SpeakIntent intent) {
    switch (intent) {
        case SpeakIntent::SD_FAIL:              return "Geheugenkaart werkt niet";
        case SpeakIntent::WIFI_FAIL:            return "WiFi werkt niet";
        case SpeakIntent::RTC_FAIL:             return "Klok werkt niet";
        case SpeakIntent::NTP_FAIL:             return "Tijd ophalen mislukt";
        case SpeakIntent::DISTANCE_SENSOR_FAIL: return "Afstandmeter werkt niet";
        case SpeakIntent::LUX_SENSOR_FAIL:      return "Lichtmeting werkt niet";
        case SpeakIntent::SENSOR3_FAIL:         return "Sensor drie ontbreekt";
        case SpeakIntent::WEATHER_FAIL:          return "Weer ophalen mislukt";
        case SpeakIntent::CALENDAR_FAIL:         return "Kalender laden mislukt";
        case SpeakIntent::DISTANCE_CLEARED:     return "Object is verdwenen";
        case SpeakIntent::WELCOME: {
            uint8_t hour = prtClock.getHour();
            if (hour < 12) return "Goedemorgen";
            if (hour < 18) return "Goedemiddag";
            return "Goedenavond";
        }
        default: return nullptr;
    }
}

// MP3 fallback: intent → max 2 woorden
struct IntentPhrase {
    SpeakIntent intent;
    uint8_t words[3];  // max 2 woorden + terminator
};

constexpr IntentPhrase phrases[] = {
    { SpeakIntent::SD_FAIL,      { MP3_SD,       MP3_END, 0 } },       // "geheugenkaart"
    { SpeakIntent::WIFI_FAIL,    { MP3_WIFI,     MP3_END, 0 } },       // "wifi"
    { SpeakIntent::RTC_FAIL,     { MP3_TIME,     MP3_FOUT, MP3_END } },// "tijd fout"
    { SpeakIntent::NTP_FAIL,     { MP3_TIME,     MP3_END, 0 } },       // "tijd"
    { SpeakIntent::DISTANCE_SENSOR_FAIL, { MP3_DISTANCE, MP3_FOUT, MP3_END } }, // "afstand fout"
    { SpeakIntent::LUX_SENSOR_FAIL, { MP3_LIGHT, MP3_FOUT, MP3_END } },        // "licht fout"
    { SpeakIntent::SENSOR3_FAIL, { MP3_SENSOR,   3, MP3_END } },       // "sensor drie"
    { SpeakIntent::WEATHER_FAIL, { MP3_TEMPERATUUR, MP3_FOUT, MP3_END } },     // "temperatuur fout"
    { SpeakIntent::CALENDAR_FAIL, { MP3_CALENDAR, MP3_FOUT, MP3_END } },       // "kalender fout"
    { SpeakIntent::DISTANCE_CLEARED, { MP3_GEEN, MP3_DISTANCE, MP3_END } },    // "geen afstand"
    // WELCOME: MP3 fallback uses goedemorgen (MP3 selection done in getWelcomeMp3)
};

// Get time-based MP3 for welcome
Mp3Id getWelcomeMp3() {
    uint8_t hour = prtClock.getHour();
    if (hour < 12) return MP3_GOEDEMORGEN;
    if (hour < 18) return MP3_GOEDEMIDDAG;
    return MP3_GOEDEAVOND;
}

const IntentPhrase* findPhrase(SpeakIntent intent) {
    for (const auto& p : phrases) {
        if (p.intent == intent) return &p;
    }
    return nullptr;
}

} // namespace

void SpeakConduct::plan() {
    // No timers to arm yet
}

void SpeakConduct::speak(SpeakIntent intent) {
    PF("[SpeakConduct] speak intent %d\n", static_cast<int>(intent));

    if (intent == SpeakIntent::SAY_TIME) return;

    // TTS primary - requires WiFi
    if (NotifyState::canPlayTTS()) {
        const char* sentence = getTtsSentence(intent);
        if (sentence) {
            PF("[SpeakConduct] TTS: %s\n", sentence);
            PlaySentence::addTTS(sentence);
            return;
        }
    }

    // MP3 fallback - alleen SD nodig
    if (!SDManager::isReady()) {
        PF("[SpeakConduct] Cannot play MP3 (no SD)\n");
        return;
    }

    if (intent == SpeakIntent::WELCOME) {
        // WELCOME uses time-based MP3
        uint8_t words[2] = { static_cast<uint8_t>(getWelcomeMp3()), MP3_END };
        PF("[SpeakConduct] MP3 fallback: welcome\n");
        PlaySentence::addWords(words);
        return;
    }
    
    const auto* phrase = findPhrase(intent);
    if (!phrase) {
        PF("[SpeakConduct] No phrase for intent %d\n", static_cast<int>(intent));
        return;
    }

    PF("[SpeakConduct] MP3 fallback\n");
    PlaySentence::addWords(phrase->words);
}

void SpeakConduct::sayTime(uint8_t hour, uint8_t minute) {
    // Gebruik scratchpad voor runtime array
    uint8_t* sentence = PlaySentence::getScratchpad();
    uint8_t idx = 0;
    
    sentence[idx++] = MP3_HET_IS;    // "het is"
    sentence[idx++] = hour;          // getal 0-23
    sentence[idx++] = MP3_UUR;       // "uur"
    
    if (minute > 0) {
        sentence[idx++] = minute;    // getal 1-59
    }
    
    sentence[idx] = MP3_END;
    
    PF("[SpeakConduct] sayTime %02u:%02u\n", hour, minute);
    PlaySentence::addWords(sentence);
}

void SpeakConduct::speakFail(StatusComponent c) {
    // Lookup table: SC_* → SpeakIntent::*_FAIL
    switch (c) {
        case SC_SD:       speak(SpeakIntent::SD_FAIL); break;
        case SC_WIFI:     speak(SpeakIntent::WIFI_FAIL); break;
        case SC_RTC:      speak(SpeakIntent::RTC_FAIL); break;
        case SC_NTP:      speak(SpeakIntent::NTP_FAIL); break;
        case SC_DIST:     speak(SpeakIntent::DISTANCE_SENSOR_FAIL); break;
        case SC_LUX:      speak(SpeakIntent::LUX_SENSOR_FAIL); break;
        case SC_SENSOR3:  speak(SpeakIntent::SENSOR3_FAIL); break;
        case SC_WEATHER:  speak(SpeakIntent::WEATHER_FAIL); break;
        case SC_CALENDAR: speak(SpeakIntent::CALENDAR_FAIL); break;
        default: break;  // SC_AUDIO, SC_TTS hebben geen FAIL intent
    }
}
