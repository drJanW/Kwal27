/**
 * @file LEDMap.cpp
 * @brief Physical LED strip mapping implementation
 * @version 260205A
 * @date 2026-02-05
 */
#include "LEDMap.h"
#include <SDController.h>
#include "HWconfig.h"  // for NUM_LEDS
#include "Globals.h"
#include <math.h>

static LEDPos ledMap[NUM_LEDS];

static void buildFallbackLEDMap() {
    const float radius = sqrtf(static_cast<float>(NUM_LEDS));
    for (int i = 0; i < NUM_LEDS; i++) {
        float angle = (2.0f * M_PI * i) / static_cast<float>(NUM_LEDS);
        ledMap[i] = {cosf(angle) * radius, sinf(angle) * radius};
    }
}

LEDPos getLEDPos(int index) {
    if (index >= 0 && index < NUM_LEDS) {
        return ledMap[index];
    }
    return {0.0f, 0.0f};
}

bool loadLEDMapFromSD(const char* path) {
    buildFallbackLEDMap();
    int loaded = 0;
    if (!path || !*path) {
        PF("[LEDMap] Invalid path\n");
        return false;
    }
    SDController::lockSD();
    File f = SD.open(path);
    if (!f) {
        PF("[LEDMap] %s not found, using fallback layout\n", path);
        SDController::unlockSD();
        return false;
    }

    for (int i = 0; i < NUM_LEDS; i++) {
        float x = 0, y = 0;
        if (f.read((uint8_t*)&x, sizeof(float)) != sizeof(float)) break;
        if (f.read((uint8_t*)&y, sizeof(float)) != sizeof(float)) break;
        ledMap[i] = {x, y};
        loaded++;
    }

    f.close();
    SDController::unlockSD();
    if (loaded != NUM_LEDS) {
        PF("[LEDMap] Loaded %d entries from %s, fallback fills remainder\n", loaded, path);
    } else {
        PF("[LEDMap] Loaded %d entries from %s\n", loaded, path);
    }
    return loaded == NUM_LEDS;
}
