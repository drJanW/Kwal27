/**
 * @file LightPatternCatalog.h
 * @brief Pattern definitions from CSV interface
 * @version 260202A
 $12026-02-05
 */
#pragma once

#include <Arduino.h>
#include <FS.h>
#include <vector>

#include "TodayModels.h"

class LightPatternCatalog {
public:
    bool begin(fs::FS& sd, const char* rootPath = "/");
    bool ready() const;
    const LightPattern* find(uint8_t id) const;
    const LightPattern* active() const;
    void clear();

private:
    bool load();
    String pathFor(const char* file) const;

    fs::FS* fs_{nullptr};
    String root_{"/"};
    bool loaded_{false};
    std::vector<LightPattern> patterns_;
    uint8_t activePatternId_{0};
};