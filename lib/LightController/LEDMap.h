/**
 * @file LEDMap.h
 * @brief Physical LED strip mapping to logical positions
 * @version 260205A
 * @date 2026-02-05
 */
#pragma once

struct LEDPos {
    float x;
    float y;
};

LEDPos getLEDPos(int index);
bool loadLEDMapFromSD(const char* path);
