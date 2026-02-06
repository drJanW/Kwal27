/**
 * @file SpeakRun.cpp
 * @brief TTS speech state management implementation
 * @version 260205A
 * @date 2026-02-05
 */
#include "SpeakRun.h"
#include "SpeakPolicy.h"
#include "SpeakWords.h"
#include "PlaySentence.h"
#include "Audio/AudioPolicy.h"
#include "Alert/AlertState.h"
#include "WiFiController.h"
#include "PRTClock.h"
#include "SDController.h"
#include "Globals.h"

namespace {

/// Get TTS sentence text for a request (primary voice output)
const char* getTtsSentence(SpeakRequest request) {
    switch (request) {
        case SpeakRequest::SD_FAIL:              return "Geheugenkaart werkt niet";
        case SpeakRequest::WIFI_FAIL:            return "WiFi werkt niet";
        case SpeakRequest::RTC_FAIL:             return "Klok werkt niet";
        case SpeakRequest::NTP_FAIL:             return "Tijd ophalen mislukt";
        case SpeakRequest::DISTANCE_SENSOR_FAIL: return "Afstandmeter werkt niet";
        case SpeakRequest::LUX_SENSOR_FAIL:      return "Lichtmeting werkt niet";
        case SpeakRequest::SENSOR3_FAIL:         return "Sensor drie ontbreekt";
        case SpeakRequest::WEATHER_FAIL:          return "Weer ophalen mislukt";
        case SpeakRequest::CALENDAR_FAIL:         return "Kalender laden mislukt";
        case SpeakRequest::DISTANCE_CLEARED:     return "Object is verdwenen";
        case SpeakRequest::WELCOME: {
            uint8_t hour = prtClock.getHour();
            if (hour < 12) return "Goedemorgen";
            if (hour < 18) return "Goedemiddag";
            return "Goedenavond";
        }
        default: return nullptr;
    }
}

/// MP3 fallback: request → max 2 words
struct RequestPhrase {
    SpeakRequest request;
    uint8_t words[3];  // max 2 words + terminator
};

constexpr RequestPhrase phrases[] = {
    { SpeakRequest::SD_FAIL,      { MP3_SD,       MP3_END, 0 } },       // "geheugenkaart"
    { SpeakRequest::WIFI_FAIL,    { MP3_WIFI,     MP3_END, 0 } },       // "wifi"
    { SpeakRequest::RTC_FAIL,     { MP3_TIME,     MP3_FOUT, MP3_END } },// "tijd fout"
    { SpeakRequest::NTP_FAIL,     { MP3_TIME,     MP3_END, 0 } },       // "tijd"
    { SpeakRequest::DISTANCE_SENSOR_FAIL, { MP3_DISTANCE, MP3_FOUT, MP3_END } }, // "afstand fout"
    { SpeakRequest::LUX_SENSOR_FAIL, { MP3_LIGHT, MP3_FOUT, MP3_END } },        // "licht fout"
    { SpeakRequest::SENSOR3_FAIL, { MP3_SENSOR,   3, MP3_END } },       // "sensor drie"
    { SpeakRequest::WEATHER_FAIL, { MP3_TEMPERATUUR, MP3_FOUT, MP3_END } },     // "temperatuur fout"
    { SpeakRequest::CALENDAR_FAIL, { MP3_CALENDAR, MP3_FOUT, MP3_END } },       // "kalender fout"
    { SpeakRequest::DISTANCE_CLEARED, { MP3_GEEN, MP3_DISTANCE, MP3_END } },    // "geen afstand"
    // WELCOME: MP3 fallback uses goedemorgen (MP3 selection done in getWelcomeMp3)
};

// Get time-based MP3 for welcome
Mp3Id getWelcomeMp3() {
    uint8_t hour = prtClock.getHour();
    if (hour < 12) return MP3_GOEDEMORGEN;
    if (hour < 18) return MP3_GOEDEMIDDAG;
    return MP3_GOEDEAVOND;
}

const RequestPhrase* findPhrase(SpeakRequest request) {
    for (const auto& p : phrases) {
        if (p.request == request) return &p;
    }
    return nullptr;
}

} // namespace

void SpeakRun::plan() {
    // No timers to arm yet
}

void SpeakRun::speak(SpeakRequest request) {
    if (request == SpeakRequest::SAY_TIME) return;

    // TTS primary - requires WiFi
    if (AlertState::canPlayTTS()) {
        const char* sentence = getTtsSentence(request);
        if (sentence) {
            PlaySentence::addTTS(sentence);
            return;
        }
    }

    // MP3 fallback - only requires SD
    if (!AlertState::isSdOk()) {
        PF("[SpeakRun] Cannot play MP3 (no SD)\n");
        return;
    }

    if (request == SpeakRequest::WELCOME) {
        // WELCOME uses time-based MP3
        uint8_t words[2] = { static_cast<uint8_t>(getWelcomeMp3()), MP3_END };
        PF("[SpeakRun] MP3 fallback: welcome\n");
        PlaySentence::addWords(words);
        return;
    }
    
    const auto* phrase = findPhrase(request);
    if (!phrase) {
        PF("[SpeakRun] No phrase for request %d\n", static_cast<int>(request));
        return;
    }

    PF("[SpeakRun] MP3 fallback\n");
    PlaySentence::addWords(phrase->words);
}

void SpeakRun::sayTime(uint8_t hour, uint8_t minute) {
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
    
    PF("[SpeakRun] sayTime %02u:%02u\n", hour, minute);
    PlaySentence::addWords(sentence);
}

void SpeakRun::speakFail(StatusComponent c) {
    // Lookup table: SC_* → SpeakRequest::*_FAIL
    switch (c) {
        case SC_SD:       speak(SpeakRequest::SD_FAIL); break;
        case SC_WIFI:     speak(SpeakRequest::WIFI_FAIL); break;
        case SC_RTC:      speak(SpeakRequest::RTC_FAIL); break;
        case SC_NTP:      speak(SpeakRequest::NTP_FAIL); break;
        case SC_DIST:     speak(SpeakRequest::DISTANCE_SENSOR_FAIL); break;
        case SC_LUX:      speak(SpeakRequest::LUX_SENSOR_FAIL); break;
        case SC_SENSOR3:  speak(SpeakRequest::SENSOR3_FAIL); break;
        case SC_WEATHER:  speak(SpeakRequest::WEATHER_FAIL); break;
        case SC_CALENDAR: speak(SpeakRequest::CALENDAR_FAIL); break;
        default: break;  // SC_AUDIO, SC_TTS have no FAIL request
    }
}
