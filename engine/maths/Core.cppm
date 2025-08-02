module;
#include <immintrin.h>
#include <cmath>
#include <numbers>

export module Math.Core;

import Core.Types;
import std;

export namespace Math {
    // Constants
    inline constexpr F32 PI = std::numbers::pi_v<F32>;
    inline constexpr F32 TWO_PI = 2.0f * PI;
    inline constexpr F32 HALF_PI = 0.5f * PI;
    inline constexpr F32 QUARTER_PI = 0.25f * PI;
    inline constexpr F32 INV_PI = std::numbers::inv_pi_v<F32>;
    inline constexpr F32 DEG_TO_RAD = PI / 180.0f;
    inline constexpr F32 RAD_TO_DEG = 180.0f / PI;
    inline constexpr F32 EPSILON = 1e-6f;
    inline constexpr F32 SQRT2 = std::numbers::sqrt2_v<F32>;
    inline constexpr F32 INV_SQRT2 = 1.0f / SQRT2;

    // Basic functions
    [[nodiscard]] constexpr F32 ToRadians(F32 degrees) { return degrees * DEG_TO_RAD; }
    [[nodiscard]] constexpr F32 ToDegrees(F32 radians) { return radians * RAD_TO_DEG; }

    [[nodiscard]] constexpr F32 Clamp(F32 value, F32 min, F32 max) {
        return value < min ? min : (value > max ? max : value);
    }

    [[nodiscard]] constexpr F32 Lerp(F32 a, F32 b, F32 t) {
        return a + (b - a) * t;
    }

    [[nodiscard]] constexpr F32 InverseLerp(F32 a, F32 b, F32 value) {
        return (value - a) / (b - a);
    }

    [[nodiscard]] constexpr F32 Remap(F32 value, F32 oldMin, F32 oldMax, F32 newMin, F32 newMax) {
        return Lerp(newMin, newMax, InverseLerp(oldMin, oldMax, value));
    }

    [[nodiscard]] constexpr F32 SmoothStep(F32 edge0, F32 edge1, F32 x) {
        x = Clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
        return x * x * (3.0f - 2.0f * x);
    }

    [[nodiscard]] constexpr F32 SmootherStep(F32 edge0, F32 edge1, F32 x) {
        x = Clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
        return x * x * x * (x * (x * 6.0f - 15.0f) + 10.0f);
    }

    [[nodiscard]] bool NearlyEqual(F32 a, F32 b, F32 epsilon = EPSILON) {
        return std::abs(a - b) <= epsilon;
    }

    [[nodiscard]] constexpr F32 Sign(F32 value) {
        return value < 0.0f ? -1.0f : (value > 0.0f ? 1.0f : 0.0f);
    }

    [[nodiscard]] inline F32 FastInvSqrt(F32 x) {
        // Using hardware intrinsic
        return _mm_cvtss_f32(_mm_rsqrt_ss(_mm_set_ss(x)));
    }

    [[nodiscard]] constexpr F32 Wrap(F32 value, F32 min, F32 max) {
        F32 range = max - min;
        while (value < min) value += range;
        while (value >= max) value -= range;
        return value;
    }

    [[nodiscard]] F32 PingPong(F32 value, F32 length) {
        value = std::abs(value);
        F32 result = std::fmod(value, length * 2.0f);
        return result > length ? 2.0f * length - result : result;
    }
}