/**
 * @file MathUtils.h
 * @brief Math utility functions (clamp, map, min/max)
 * @version 251231E
 * @date 2025-12-31
 *
 * This header provides type-safe math utility functions and constants.
 * Includes template-based min/max/clamp functions that avoid the double
 * evaluation problem of Arduino macros, range mapping functions for
 * linear interpolation, mathematical constants (pi, 2pi), and specialized
 * functions for color/brightness calculations used in LED animations.
 */

#pragma once

#include <Arduino.h>
#include <cmath>
#include <type_traits>

namespace MathUtils {

constexpr float kPi = 3.14159265358979323846f;
constexpr float k2Pi = 2.0f * kPi;

namespace detail {
    template<typename T>
    struct IsArithmetic {
        static constexpr bool value = std::is_arithmetic<T>::value;
    };
}

// ─────────────────────────────────────────────────────────────
// Type-safe min/max - prefer over Arduino macros (no double eval)
// ─────────────────────────────────────────────────────────────
template <typename T,
          typename std::enable_if<detail::IsArithmetic<T>::value, int>::type = 0>
inline T minVal(T a, T b) {
    return (a < b) ? a : b;
}

template <typename T,
          typename std::enable_if<detail::IsArithmetic<T>::value, int>::type = 0>
inline T maxVal(T a, T b) {
    return (a > b) ? a : b;
}

template <typename T,
          typename std::enable_if<detail::IsArithmetic<T>::value, int>::type = 0>
inline T clamp(T value, T minValue, T maxValue) {
    if (minValue > maxValue) {
        const T tmp = minValue;
        minValue = maxValue;
        maxValue = tmp;
    }
    if (value < minValue) {
        return minValue;
    }
    if (value > maxValue) {
        return maxValue;
    }
    return value;
}

template <typename T,
          typename std::enable_if<detail::IsArithmetic<T>::value, int>::type = 0>
inline T map(T value, T inMin, T inMax, T outMin, T outMax) {
    if (inMin == inMax) {
        return outMin;
    }
    const T t = (value - inMin) / (inMax - inMin);
    return outMin + t * (outMax - outMin);
}

inline float clamp01(float value) {
    return clamp(value, 0.0f, 1.0f);
}

inline double clamp01(double value) {
    return clamp(value, 0.0, 1.0);
}

inline float wrap(float value, float minValue, float maxValue) {
    if (!(maxValue > minValue)) {
        return minValue;
    }
    const float span = maxValue - minValue;
    float wrapped = std::fmod(value - minValue, span);
    if (wrapped < 0.0f) {
        wrapped += span;
    }
    return wrapped + minValue;
}

inline float wrap01(float value) {
    return wrap(value, 0.0f, 1.0f);
}

inline float wrapAngleRadians(float radians) {
    return wrap(radians, -kPi, kPi);
}

inline float wrapAngleDegrees(float degrees) {
    return wrap(degrees, -180.0f, 180.0f);
}

inline float lerp(float a, float b, float t) {
    return a + (b - a) * clamp01(t);
}

inline float inverseLerp(float a, float b, float value) {
    if (a == b) {
        return 0.0f;
    }
    return clamp01((value - a) / (b - a));
}

inline float mapRange(float value,
                      float inMin,
                      float inMax,
                      float outMin,
                      float outMax) {
    const float t = inverseLerp(inMin, inMax, value);
    return lerp(outMin, outMax, t);
}

inline bool nearlyEqual(float a, float b, float epsilon = 1e-5f) {
    return fabsf(a - b) <= epsilon;
}

inline float applyDeadband(float value, float threshold) {
    const float absValue = fabsf(value);
    if (absValue <= threshold) {
        return 0.0f;
    }
    return (value > 0.0f ? 1.0f : -1.0f) * (absValue - threshold);
}

inline bool applyHysteresis(bool currentState,
                            float value,
                            float turnOnThreshold,
                            float turnOffThreshold) {
    if (currentState) {
        return value > turnOffThreshold;
    }
    return value > turnOnThreshold;
}

} // namespace MathUtils
