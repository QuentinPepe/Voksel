module;
#include <immintrin.h>
#include <cmath>

export module Math.Vector;

import Core.Types;
import Core.Assert;
import Math.Core;
import std;

export namespace Math {
    struct alignas(8) Vec2 {
        union {
            struct { F32 x, y; };
            F32 data[2];
        };

        constexpr Vec2() : x{0.0f}, y{0.0f} {}
        constexpr Vec2(F32 scalar) : x{scalar}, y{scalar} {}
        constexpr Vec2(F32 x_, F32 y_) : x{x_}, y{y_} {}

        [[nodiscard]] constexpr Vec2 operator+(const Vec2& other) const { return {x + other.x, y + other.y}; }
        [[nodiscard]] constexpr Vec2 operator-(const Vec2& other) const { return {x - other.x, y - other.y}; }
        [[nodiscard]] constexpr Vec2 operator*(F32 scalar) const { return {x * scalar, y * scalar}; }
        [[nodiscard]] constexpr Vec2 operator/(F32 scalar) const { return {x / scalar, y / scalar}; }
        [[nodiscard]] constexpr Vec2 operator-() const { return {-x, -y}; }

        constexpr Vec2& operator+=(const Vec2& other) { x += other.x; y += other.y; return *this; }
        constexpr Vec2& operator-=(const Vec2& other) { x -= other.x; y -= other.y; return *this; }
        constexpr Vec2& operator*=(F32 scalar) { x *= scalar; y *= scalar; return *this; }
        constexpr Vec2& operator/=(F32 scalar) { x /= scalar; y /= scalar; return *this; }

        [[nodiscard]] constexpr bool operator==(const Vec2& other) const { return x == other.x && y == other.y; }
        [[nodiscard]] constexpr bool operator!=(const Vec2& other) const { return !(*this == other); }

        [[nodiscard]] constexpr F32 operator[](USize i) const { assert(i < 2, "Vec2 index out of bounds"); return data[i]; }
        [[nodiscard]] constexpr F32& operator[](USize i) { assert(i < 2, "Vec2 index out of bounds"); return data[i]; }

        [[nodiscard]] constexpr F32 Dot(const Vec2& other) const { return x * other.x + y * other.y; }
        [[nodiscard]] F32 Length() const { return std::sqrt(LengthSquared()); }
        [[nodiscard]] constexpr F32 LengthSquared() const { return x * x + y * y; }
        [[nodiscard]] Vec2 Normalized() const { F32 len = Length(); return len > 0 ? *this / len : Vec2{}; }

        void Normalize() { *this = Normalized(); }

        [[nodiscard]] constexpr Vec2 Perpendicular() const { return {-y, x}; }
        [[nodiscard]] static constexpr Vec2 Lerp(const Vec2& a, const Vec2& b, F32 t) {
            return {Math::Lerp(a.x, b.x, t), Math::Lerp(a.y, b.y, t)};
        }

        static const Vec2 Zero;
        static const Vec2 One;
        static const Vec2 UnitX;
        static const Vec2 UnitY;
    };

    inline constexpr Vec2 Vec2::Zero{0.0f, 0.0f};
    inline constexpr Vec2 Vec2::One{1.0f, 1.0f};
    inline constexpr Vec2 Vec2::UnitX{1.0f, 0.0f};
    inline constexpr Vec2 Vec2::UnitY{0.0f, 1.0f};

    [[nodiscard]] constexpr Vec2 operator*(F32 scalar, const Vec2& vec) { return vec * scalar; }

    struct alignas(16) Vec3 {
        union {
            struct { F32 x, y, z; };
            F32 data[3];
        };

        constexpr Vec3() : x{0.0f}, y{0.0f}, z{0.0f} {}
        constexpr Vec3(F32 scalar) : x{scalar}, y{scalar}, z{scalar} {}
        constexpr Vec3(F32 x_, F32 y_, F32 z_) : x{x_}, y{y_}, z{z_} {}
        constexpr Vec3(const Vec2& xy, F32 z_) : x{xy.x}, y{xy.y}, z{z_} {}

        [[nodiscard]] constexpr Vec3 operator+(const Vec3& other) const { return {x + other.x, y + other.y, z + other.z}; }
        [[nodiscard]] constexpr Vec3 operator-(const Vec3& other) const { return {x - other.x, y - other.y, z - other.z}; }
        [[nodiscard]] constexpr Vec3 operator*(const Vec3& other) const { return {x * other.x, y * other.y, z * other.z};  }
        [[nodiscard]] constexpr Vec3 operator*(F32 scalar) const { return {x * scalar, y * scalar, z * scalar}; }
        [[nodiscard]] constexpr Vec3 operator/(F32 scalar) const { return {x / scalar, y / scalar, z / scalar}; }
        [[nodiscard]] constexpr Vec3 operator-() const { return {-x, -y, -z}; }


        constexpr Vec3& operator+=(const Vec3& other) { x += other.x; y += other.y; z += other.z; return *this; }
        constexpr Vec3& operator-=(const Vec3& other) { x -= other.x; y -= other.y; z -= other.z; return *this; }
        constexpr Vec3& operator*=(F32 scalar) { x *= scalar; y *= scalar; z *= scalar; return *this; }
        constexpr Vec3& operator/=(F32 scalar) { x /= scalar; y /= scalar; z /= scalar; return *this; }

        [[nodiscard]] constexpr bool operator==(const Vec3& other) const { return x == other.x && y == other.y && z == other.z; }
        [[nodiscard]] constexpr bool operator!=(const Vec3& other) const { return !(*this == other); }

        [[nodiscard]] constexpr F32 operator[](USize i) const { assert(i < 3, "Vec3 index out of bounds"); return data[i]; }
        [[nodiscard]] constexpr F32& operator[](USize i) { assert(i < 3, "Vec3 index out of bounds"); return data[i]; }

        [[nodiscard]] constexpr F32 Dot(const Vec3& other) const { return x * other.x + y * other.y + z * other.z; }
        [[nodiscard]] constexpr Vec3 Cross(const Vec3& other) const {
            return {y * other.z - z * other.y, z * other.x - x * other.z, x * other.y - y * other.x};
        }

        [[nodiscard]] F32 Length() const { return std::sqrt(LengthSquared()); }
        [[nodiscard]] constexpr F32 LengthSquared() const { return x * x + y * y + z * z; }
        [[nodiscard]] Vec3 Normalized() const { F32 len = Length(); return len > 0 ? *this / len : Vec3{}; }

        void Normalize() { *this = Normalized(); }

        [[nodiscard]] static constexpr Vec3 Lerp(const Vec3& a, const Vec3& b, F32 t) {
            return {Math::Lerp(a.x, b.x, t), Math::Lerp(a.y, b.y, t), Math::Lerp(a.z, b.z, t)};
        }

        [[nodiscard]] static Vec3 Slerp(const Vec3& a, const Vec3& b, F32 t) {
            F32 dot = Clamp(a.Dot(b), -1.0f, 1.0f);
            F32 theta = std::acos(dot) * t;
            Vec3 relative = (b - a * dot).Normalized();
            return a * std::cos(theta) + relative * std::sin(theta);
        }

        [[nodiscard]] constexpr Vec2 XY() const { return {x, y}; }
        [[nodiscard]] constexpr Vec2 XZ() const { return {x, z}; }
        [[nodiscard]] constexpr Vec2 YZ() const { return {y, z}; }

        static const Vec3 Zero;
        static const Vec3 One;
        static const Vec3 UnitX;
        static const Vec3 UnitY;
        static const Vec3 UnitZ;
        static const Vec3 Up;
        static const Vec3 Down;
        static const Vec3 Left;
        static const Vec3 Right;
        static const Vec3 Forward;
        static const Vec3 Back;
    };

    inline constexpr Vec3 Vec3::Zero{0.0f, 0.0f, 0.0f};
    inline constexpr Vec3 Vec3::One{1.0f, 1.0f, 1.0f};
    inline constexpr Vec3 Vec3::UnitX{1.0f, 0.0f, 0.0f};
    inline constexpr Vec3 Vec3::UnitY{0.0f, 1.0f, 0.0f};
    inline constexpr Vec3 Vec3::UnitZ{0.0f, 0.0f, 1.0f};
    inline constexpr Vec3 Vec3::Up{0.0f, 1.0f, 0.0f};
    inline constexpr Vec3 Vec3::Down{0.0f, -1.0f, 0.0f};
    inline constexpr Vec3 Vec3::Left{-1.0f, 0.0f, 0.0f};
    inline constexpr Vec3 Vec3::Right{1.0f, 0.0f, 0.0f};
    inline constexpr Vec3 Vec3::Forward{0.0f, 0.0f, -1.0f};
    inline constexpr Vec3 Vec3::Back{0.0f, 0.0f, 1.0f};

    [[nodiscard]] constexpr Vec3 operator*(F32 scalar, const Vec3& vec) { return vec * scalar; }

    struct alignas(16) Vec4 {
        union {
            struct { F32 x, y, z, w; };
            F32 data[4];
            __m128 simd;
        };

        constexpr Vec4() : x{0.0f}, y{0.0f}, z{0.0f}, w{0.0f} {}
        constexpr Vec4(F32 scalar) : x{scalar}, y{scalar}, z{scalar}, w{scalar} {}
        constexpr Vec4(F32 x_, F32 y_, F32 z_, F32 w_) : x{x_}, y{y_}, z{z_}, w{w_} {}
        constexpr Vec4(const Vec3& xyz, F32 w_) : x{xyz.x}, y{xyz.y}, z{xyz.z}, w{w_} {}
        constexpr Vec4(const Vec2& xy, const Vec2& zw) : x{xy.x}, y{xy.y}, z{zw.x}, w{zw.y} {}
        Vec4(__m128 v) : simd{v} {}

        [[nodiscard]] inline Vec4 operator+(const Vec4& other) const {
            return Vec4{_mm_add_ps(simd, other.simd)};
        }

        [[nodiscard]] inline Vec4 operator-(const Vec4& other) const {
            return Vec4{_mm_sub_ps(simd, other.simd)};
        }

        [[nodiscard]] inline Vec4 operator*(F32 scalar) const {
            return Vec4{_mm_mul_ps(simd, _mm_set1_ps(scalar))};
        }

        [[nodiscard]] inline Vec4 operator/(F32 scalar) const {
            return Vec4{_mm_div_ps(simd, _mm_set1_ps(scalar))};
        }

        [[nodiscard]] inline Vec4 operator-() const {
            return Vec4{_mm_sub_ps(_mm_setzero_ps(), simd)};
        }

        inline Vec4& operator+=(const Vec4& other) { simd = _mm_add_ps(simd, other.simd); return *this; }
        inline Vec4& operator-=(const Vec4& other) { simd = _mm_sub_ps(simd, other.simd); return *this; }
        inline Vec4& operator*=(F32 scalar) { simd = _mm_mul_ps(simd, _mm_set1_ps(scalar)); return *this; }
        inline Vec4& operator/=(F32 scalar) { simd = _mm_div_ps(simd, _mm_set1_ps(scalar)); return *this; }

        [[nodiscard]] constexpr bool operator==(const Vec4& other) const {
            return x == other.x && y == other.y && z == other.z && w == other.w;
        }
        [[nodiscard]] constexpr bool operator!=(const Vec4& other) const { return !(*this == other); }

        [[nodiscard]] constexpr F32 operator[](USize i) const { assert(i < 4, "Vec4 index out of bounds"); return data[i]; }
        [[nodiscard]] constexpr F32& operator[](USize i) { assert(i < 4, "Vec4 index out of bounds"); return data[i]; }

        [[nodiscard]] inline F32 Dot(const Vec4& other) const {
            __m128 mul = _mm_mul_ps(simd, other.simd);
            __m128 hadd = _mm_hadd_ps(mul, mul);
            hadd = _mm_hadd_ps(hadd, hadd);
            return _mm_cvtss_f32(hadd);
        }

        [[nodiscard]] F32 Length() const { return std::sqrt(LengthSquared()); }
        [[nodiscard]] inline F32 LengthSquared() const { return Dot(*this); }
        [[nodiscard]] Vec4 Normalized() const { F32 len = Length(); return len > 0 ? *this / len : Vec4{}; }

        void Normalize() { *this = Normalized(); }

        [[nodiscard]] static Vec4 Lerp(const Vec4& a, const Vec4& b, F32 t) {
            __m128 t_vec = _mm_set1_ps(t);
            return Vec4{_mm_add_ps(a.simd, _mm_mul_ps(_mm_sub_ps(b.simd, a.simd), t_vec))};
        }

        [[nodiscard]] constexpr Vec3 XYZ() const { return {x, y, z}; }
        [[nodiscard]] constexpr Vec2 XY() const { return {x, y}; }
        [[nodiscard]] constexpr Vec2 ZW() const { return {z, w}; }

        static const Vec4 Zero;
        static const Vec4 One;
        static const Vec4 UnitX;
        static const Vec4 UnitY;
        static const Vec4 UnitZ;
        static const Vec4 UnitW;
    };

    inline constexpr Vec4 Vec4::Zero{0.0f, 0.0f, 0.0f, 0.0f};
    inline constexpr Vec4 Vec4::One{1.0f, 1.0f, 1.0f, 1.0f};
    inline constexpr Vec4 Vec4::UnitX{1.0f, 0.0f, 0.0f, 0.0f};
    inline constexpr Vec4 Vec4::UnitY{0.0f, 1.0f, 0.0f, 0.0f};
    inline constexpr Vec4 Vec4::UnitZ{0.0f, 0.0f, 1.0f, 0.0f};
    inline constexpr Vec4 Vec4::UnitW{0.0f, 0.0f, 0.0f, 1.0f};

    [[nodiscard]] inline Vec4 operator*(F32 scalar, const Vec4& vec) { return vec * scalar; }

    struct IVec2 {
        S32 x, y;

        constexpr IVec2() : x{0}, y{0} {}
        constexpr IVec2(S32 scalar) : x{scalar}, y{scalar} {}
        constexpr IVec2(S32 x_, S32 y_) : x{x_}, y{y_} {}

        [[nodiscard]] constexpr IVec2 operator+(const IVec2& other) const { return {x + other.x, y + other.y}; }
        [[nodiscard]] constexpr IVec2 operator-(const IVec2& other) const { return {x - other.x, y - other.y}; }
        [[nodiscard]] constexpr IVec2 operator*(S32 scalar) const { return {x * scalar, y * scalar}; }
        [[nodiscard]] constexpr IVec2 operator/(S32 scalar) const { return {x / scalar, y / scalar}; }
        [[nodiscard]] constexpr IVec2 operator-() const { return {-x, -y}; }

        [[nodiscard]] constexpr bool operator==(const IVec2& other) const { return x == other.x && y == other.y; }
        [[nodiscard]] constexpr bool operator!=(const IVec2& other) const { return !(*this == other); }

        [[nodiscard]] constexpr Vec2 ToFloat() const { return {static_cast<F32>(x), static_cast<F32>(y)}; }
    };

    struct IVec3 {
        S32 x, y, z;

        constexpr IVec3() : x{0}, y{0}, z{0} {}
        constexpr IVec3(S32 scalar) : x{scalar}, y{scalar}, z{scalar} {}
        constexpr IVec3(S32 x_, S32 y_, S32 z_) : x{x_}, y{y_}, z{z_} {}

        [[nodiscard]] constexpr IVec3 operator+(const IVec3& other) const { return {x + other.x, y + other.y, z + other.z}; }
        [[nodiscard]] constexpr IVec3 operator-(const IVec3& other) const { return {x - other.x, y - other.y, z - other.z}; }
        [[nodiscard]] constexpr IVec3 operator*(S32 scalar) const { return {x * scalar, y * scalar, z * scalar}; }
        [[nodiscard]] constexpr IVec3 operator/(S32 scalar) const { return {x / scalar, y / scalar, z / scalar}; }
        [[nodiscard]] constexpr IVec3 operator-() const { return {-x, -y, -z}; }

        [[nodiscard]] constexpr bool operator==(const IVec3& other) const { return x == other.x && y == other.y && z == other.z; }
        [[nodiscard]] constexpr bool operator!=(const IVec3& other) const { return !(*this == other); }

        [[nodiscard]] constexpr Vec3 ToFloat() const { return {static_cast<F32>(x), static_cast<F32>(y), static_cast<F32>(z)}; }
    };
}