/**
 * @file ThemeBoxTable.h
 * @brief Audio theme box table interface
 * @version 260202A
 * @date 2026-02-02
 */
#pragma once

#include <Arduino.h>
#include <FS.h>
#include <vector>

#include "TodayModels.h"

class ThemeBoxTable {
public:
    bool begin(fs::FS& sd, const char* rootPath = "/");
    bool ready() const;
    const ThemeBox* find(uint8_t id) const;
    const ThemeBox* active() const;
    const std::vector<ThemeBox>& boxes() const { return boxes_; }
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
