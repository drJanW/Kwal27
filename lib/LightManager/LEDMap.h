/**
 * @file LEDMap.h
 * @brief Physical LED strip mapping to logical positions
 * @version 251231F
 * @date 2025-12-31
 *
 * Header file defining the LED position mapping interface. Provides structures
 * and functions to translate between physical LED indices on the strip and
 * their logical 2D coordinates. Supports loading custom position maps from
 * binary SD card file or generating fallback circular layouts.
 */

#ifndef LEDMAP_H
#define LEDMAP_H

struct LEDPos {
    float x;
    float y;
};

LEDPos getLEDPos(int index);
bool loadLEDMapFromSD(const char* path);

#endif
