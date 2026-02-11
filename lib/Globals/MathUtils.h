/**
 * @file MathUtils.h
 * @brief Math utility functions (clamp, map, min/max)
 * @version 260127A
 $12026-02-09
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

template <typename T1, typename T2, typename T3>
inline auto clamp(T1 value, T2 minValue, T3 maxValue)
    -> typename std::common_type<T1, T2, T3>::type {
    using R = typename std::common_type<T1, T2, T3>::type;
    R lo = static_cast<R>(minValue);
    R hi = static_cast<R>(maxValue);
    if (lo > hi) { R tmp = lo; lo = hi; hi = tmp; }
    R v = static_cast<R>(value);
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

template <typename T1, typename T2, typename T3, typename T4, typename T5>
inline float map(T1 value, T2 inMin, T3 inMax, T4 outMin, T5 outMax) {
    float fInMin = static_cast<float>(inMin);
    float fInMax = static_cast<float>(inMax);
    if (fInMin == fInMax) return static_cast<float>(outMin);
    float t = (static_cast<float>(value) - fInMin) / (fInMax - fInMin);
    return static_cast<float>(outMin) + t * (static_cast<float>(outMax) - static_cast<float>(outMin));
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

template <typename T1, typename T2, typename T3, typename T4, typename T5>
inline float mapRange(T1 value, T2 inMin, T3 inMax, T4 outMin, T5 outMax) {
    const float t = inverseLerp(static_cast<float>(inMin), static_cast<float>(inMax), static_cast<float>(value));
    return lerp(static_cast<float>(outMin), static_cast<float>(outMax), t);
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
