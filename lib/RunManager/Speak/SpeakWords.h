/**
 * @file SpeakWords.h
 * @brief MP3 word ID definitions for TTS
 * @version 251231E
 * @date 2025-12-31
 *
 * Defines MP3 file IDs for the modular word-based TTS system. Words are
 * stored as /000/XXX.mp3 files and combined to form sentences. Includes
 * Dutch numbers (0-59), subjects, states, and modifiers.
 */

// SpeakWords.h
// MP3 IDs for speak audio files in /000/
#pragma once
#include <stdint.h>

// MP3 files stored as /000/XXX.mp3 where XXX = 3-digit ID
// Modular word system: subjects + states + modifiers = flexible sentences
// SD_MAX_FILES_PER_SUBDIR = 101, so max file is 100.mp3

enum Mp3Id : uint8_t {
    // ============================================
    // Numbers 0-59: spoken Dutch numbers
    // Used for hours (0-23) and minutes (0-59)
    // ============================================
    MP3_ZERO        = 0,    // "nul"
    MP3_ONE         = 1,    // "een"
    MP3_TWO         = 2,    // "twee"
    // ... 3-59: drie t/m negenenvijftig

    // ============================================
    // Subjects 60-72: onderwerpen
    // ============================================
    MP3_SYSTEM      = 60,   // "systeem"
    MP3_WIFI        = 61,   // "wifi"
    MP3_SENSOR      = 62,   // "sensor"
    MP3_TEMPERATUUR = 63,   // "temperatuur"
    MP3_SD          = 64,   // "geheugenkaart"
    MP3_OTA         = 65,   // "update"
    MP3_WEB         = 66,   // "webinterface"
    MP3_AUDIO       = 67,   // "audio"
    MP3_LIGHT       = 68,   // "licht"
    MP3_DISTANCE    = 69,   // "afstand"
    MP3_CALENDAR    = 70,   // "kalender"
    MP3_TIME        = 71,   // "tijd"
    MP3_POWER       = 72,   // "voeding"

    // ============================================
    // States/actions 73-83: staten en acties
    // ============================================
    MP3_START       = 73,   // "start"
    MP3_GEREED      = 74,   // "gereed"
    MP3_BEZIG       = 75,   // "bezig"
    MP3_FOUT        = 76,   // "fout"
    MP3_MISLUKT     = 77,   // "mislukt"
    MP3_OPNIEUW     = 78,   // "opnieuw"
    MP3_VERBINDING  = 79,   // "verbinding"
    MP3_UPDATE      = 80,   // "update"
    MP3_CONTROLE    = 81,   // "controle"
    MP3_HERSTELD    = 82,   // "hersteld"
    MP3_BESCHIKBAAR = 83,   // "beschikbaar"

    // ============================================
    // Modifiers 84-90: bijwoorden/bijvoeglijk
    // ============================================
    MP3_GEEN        = 84,   // "geen"
    MP3_LAGE        = 85,   // "lage"
    MP3_HOGE        = 86,   // "hoge"
    MP3_KRITIEK     = 87,   // "kritiek"
    MP3_INFORMATIE  = 88,   // "informatie"
    MP3_MELDING     = 89,   // "melding"
    MP3_AUTOMATISCH = 90,   // "automatisch"

    // ============================================
    // Time building blocks 91-100
    // For constructing sentences like "het is X uur Y"
    // ============================================
    MP3_HET_IS      = 91,   // "het is"
    MP3_UUR         = 92,   // "uur"
    MP3_MINUTEN     = 93,   // "minuten"
    MP3_GRADEN      = 94,   // "graden"
    MP3_PROCENT     = 95,   // "procent"
    MP3_EN          = 96,   // "en"
    MP3_GOEDEMORGEN = 97,   // "goedemorgen"
    MP3_GOEDEMIDDAG = 98,   // "goedemiddag"
    MP3_GOEDEAVOND  = 99,   // "goedeavond"
    MP3_GOEDENACHT  = 100,  // "goedenacht"

    // End of sentence marker (special value, not a file)
    MP3_END         = 255
};
