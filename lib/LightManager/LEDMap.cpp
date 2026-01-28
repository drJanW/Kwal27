/**
 * @file LEDMap.cpp
 * @brief Physical LED strip mapping implementation
 * @version 251231F
 * @date 2025-12-31
 *
 * Implementation of LED position mapping from physical strip indices to logical
 * 2D coordinates. Loads position data from binary file on SD card when available
 * (pairs of floats for x,y coordinates), or generates a fallback circular layout
 * based on LED count. Used for spatial lighting effects and position-aware
 * color patterns.
 */

#include "LEDMap.h"
#include <SDManager.h>
#include "HWconfig.h"  // voor NUM_LEDS
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
    SDManager::lockSD();
    File f = SD.open(path);
    if (!f) {
        PF("[LEDMap] %s not found, using fallback layout\n", path);
        SDManager::unlockSD();
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
    SDManager::unlockSD();
    if (loaded != NUM_LEDS) {
        PF("[LEDMap] Loaded %d entries from %s, fallback fills remainder\n", loaded, path);
    } else {
        PF("[LEDMap] Loaded %d entries from %s\n", loaded, path);
    }
    return loaded == NUM_LEDS;
}
