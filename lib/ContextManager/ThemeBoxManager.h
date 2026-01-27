/**
 * @file ThemeBoxManager.h
 * @brief Audio theme box management interface
 * @version 251231E
 * @date 2025-12-31
 *
 * Defines the ThemeBoxManager class for managing audio theme boxes.
 * Provides interface for loading theme boxes from CSV, looking up
 * theme boxes by ID, accessing the currently active theme box,
 * and clearing the theme box store. Theme boxes organize audio
 * content into themed collections.
 */

#pragma once

#include <Arduino.h>
#include <FS.h>
#include <vector>

#include "ContextModels.h"

class ThemeBoxManager {
public:
    bool begin(fs::FS& sd, const char* rootPath = "/");
    bool ready() const;
    const ThemeBox* find(uint8_t id) const;
    const ThemeBox* active() const;
    void clear();

private:
    bool load();
    String pathFor(const char* file) const;

    fs::FS* fs_{nullptr};
    String root_{"/"};
    bool loaded_{false};
    std::vector<ThemeBox> boxes_;
    uint8_t activeThemeBoxId_{0};
};
