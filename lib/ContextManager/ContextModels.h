#pragma once

#include <Arduino.h>
#include <vector>

#include "Calendar.h"

struct ThemeBox {
    bool valid{false};
    uint8_t id{0};
    String name;
    std::vector<uint16_t> entries;
};

struct LightPattern {
    bool valid{false};
    uint8_t id{0};
    String label;
    float color_cycle_sec{0.0f};
    float bright_cycle_sec{0.0f};
    float fade_width{0.0f};
    float min_brightness{0.0f};
    float gradient_speed{0.0f};
    float center_x{0.0f};
    float center_y{0.0f};
    float radius{0.0f};
    float window_width{0.0f};
    float radius_osc{0.0f};
    float x_amp{0.0f};
    float y_amp{0.0f};
    float x_cycle_sec{0.0f};
    float y_cycle_sec{0.0f};
};

struct RgbColor {
    uint8_t r{0};
    uint8_t g{0};
    uint8_t b{0};
};

struct LightColor {
    bool valid{false};
    uint8_t id{0};
    String label;
    RgbColor colorA;
    RgbColor colorB;
};

struct TodayContext {
    bool valid{false};
    CalendarEntry entry;
    ThemeBox theme;
    LightPattern pattern;
    LightColor colors;
};
