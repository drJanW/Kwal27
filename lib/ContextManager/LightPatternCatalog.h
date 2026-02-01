/**
 * @file LightPatternCatalog.h
 * @brief Pattern definitions from CSV interface
 * @version 251231E
 * @date 2025-12-31
 *
 * Defines the LightPatternCatalog class for managing light pattern definitions.
 * Provides interface for loading patterns from CSV, looking up patterns by ID,
 * accessing the currently active pattern, and clearing the pattern catalog.
 * Patterns define LED animation behavior and timing.
 */

#pragma once

#include <Arduino.h>
#include <FS.h>
#include <vector>

#include "ContextModels.h"

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