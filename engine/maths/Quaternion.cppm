module;
#include <immintrin.h>
#include <cmath>

export module Math.Quaternion;

import Core.Types;
import Core.Assert;
import Math.Core;
import Math.Vector;
import Math.Matrix;
import std;

export namespace Math {
    struct alignas(16) Quat {
        union {
            struct { F32 x, y, z, w; };
            Vec4 vec;
            __m128 simd;
        };

        constexpr Quat() : x{0.0f}, y{0.0f}, z{0.0f}, w{1.0f} {}
        constexpr Quat(F32 x_, F32 y_, F32 z_, F32 w_) : x{x_}, y{y_}, z{z_}, w{w_} {}
        constexpr Quat(const Vec3& v, F32 scalar) : x{v.x}, y{v.y}, z{v.z}, w{scalar} {}
        constexpr explicit Quat(const Vec4& v) : x{v.x}, y{v.y}, z{v.z}, w{v.w} {}
        Quat(__m128 v) : simd{v} {}

        [[nodiscard]] static Quat FromAxisAngle(const Vec3& axis, F32 angle) {
            F32 halfAngle = angle * 0.5f;
            F32 s = std::sin(halfAngle);
            Vec3 v = axis.Normalized() * s;
            return Quat{v, std::cos(halfAngle)};
        }

        [[nodiscard]] static Quat FromEuler(F32 pitch, F32 yaw, F32 roll) {
            F32 cy = std::cos(yaw * 0.5f);
            F32 sy = std::sin(yaw * 0.5f);
            F32 cp = std::cos(pitch * 0.5f);
            F32 sp = std::sin(pitch * 0.5f);
            F32 cr = std::cos(roll * 0.5f);
            F32 sr = std::sin(roll * 0.5f);

            return Quat{
                sr * cp * cy - cr * sp * sy,
                cr * sp * cy + sr * cp * sy,
                cr * cp * sy - sr * sp * cy,
                cr * cp * cy + sr * sp * sy
            };
        }

        [[nodiscard]] static Quat FromEuler(const Vec3& euler) {
            return FromEuler(euler.x, euler.y, euler.z);
        }

        [[nodiscard]] static Quat FromRotationMatrix(const Mat3& m) {
            F32 trace = m.m[0][0] + m.m[1][1] + m.m[2][2];

            if (trace > 0.0f) {
                F32 s = 0.5f / std::sqrt(trace + 1.0f);
                return Quat{
                    (m.m[1][2] - m.m[2][1]) * s,
                    (m.m[2][0] - m.m[0][2]) * s,
                    (m.m[0][1] - m.m[1][0]) * s,
                    0.25f / s
                };
            } else if (m.m[0][0] > m.m[1][1] && m.m[0][0] > m.m[2][2]) {
                F32 s = 2.0f * std::sqrt(1.0f + m.m[0][0] - m.m[1][1] - m.m[2][2]);
                return Quat{
                    0.25f * s,
                    (m.m[1][0] + m.m[0][1]) / s,
                    (m.m[2][0] + m.m[0][2]) / s,
                    (m.m[1][2] - m.m[2][1]) / s
                };
            } else if (m.m[1][1] > m.m[2][2]) {
                F32 s = 2.0f * std::sqrt(1.0f + m.m[1][1] - m.m[0][0] - m.m[2][2]);
                return Quat{
                    (m.m[1][0] + m.m[0][1]) / s,
                    0.25f * s,
                    (m.m[2][1] + m.m[1][2]) / s,
                    (m.m[2][0] - m.m[0][2]) / s
                };
            } else {
                F32 s = 2.0f * std::sqrt(1.0f + m.m[2][2] - m.m[0][0] - m.m[1][1]);
                return Quat{
                    (m.m[2][0] + m.m[0][2]) / s,
                    (m.m[2][1] + m.m[1][2]) / s,
                    0.25f * s,
                    (m.m[0][1] - m.m[1][0]) / s
                };
            }
        }

        [[nodiscard]] static Quat LookRotation(const Vec3& forward, const Vec3& up = Vec3::Up) {
            Vec3 f = forward.Normalized();
            Vec3 r = up.Cross(f).Normalized();
            Vec3 u = f.Cross(r);

            Mat3 m{
                r.x, r.y, r.z,
                u.x, u.y, u.z,
                -f.x, -f.y, -f.z
            };

            return FromRotationMatrix(m);
        }

        [[nodiscard]] inline Quat operator*(const Quat& other) const {
            __m128 q1 = simd;
            __m128 q2 = other.simd;

            // Expand quaternion multiplication
            __m128 q1_wwww = _mm_shuffle_ps(q1, q1, _MM_SHUFFLE(3, 3, 3, 3));
            __m128 q1_xyzx = _mm_shuffle_ps(q1, q1, _MM_SHUFFLE(0, 2, 1, 0));
            __m128 q1_yzxy = _mm_shuffle_ps(q1, q1, _MM_SHUFFLE(1, 0, 2, 1));
            __m128 q1_zxyz = _mm_shuffle_ps(q1, q1, _MM_SHUFFLE(2, 1, 0, 2));

            __m128 q2_xyzw = q2;
            __m128 q2_yzxw = _mm_shuffle_ps(q2, q2, _MM_SHUFFLE(3, 0, 2, 1));
            __m128 q2_zxyw = _mm_shuffle_ps(q2, q2, _MM_SHUFFLE(3, 1, 0, 2));
            __m128 q2_wzyx = _mm_shuffle_ps(q2, q2, _MM_SHUFFLE(0, 1, 2, 3));

            __m128 result = _mm_mul_ps(q1_wwww, q2_xyzw);
            result = _mm_add_ps(result, _mm_mul_ps(q1_xyzx, q2_wzyx));
            result = _mm_sub_ps(result, _mm_mul_ps(q1_yzxy, q2_zxyw));
            result = _mm_add_ps(result, _mm_mul_ps(q1_zxyz, q2_yzxw));

            return Quat{result};
        }

        [[nodiscard]] Vec3 operator*(const Vec3& v) const {
            Vec3 qv{x, y, z};
            Vec3 t = 2.0f * qv.Cross(v);
            return v + w * t + qv.Cross(t);
        }

        [[nodiscard]] inline Quat Conjugate() const {
            return Quat{_mm_mul_ps(simd, _mm_set_ps(1.0f, -1.0f, -1.0f, -1.0f))};
        }

        [[nodiscard]] inline F32 Dot(const Quat& other) const {
            return vec.Dot(other.vec);
        }

        [[nodiscard]] F32 Length() const { return vec.Length(); }
        [[nodiscard]] F32 LengthSquared() const { return vec.LengthSquared(); }

        [[nodiscard]] Quat Normalized() const {
            F32 len = Length();
            return len > 0 ? Quat{vec / len} : Quat{};
        }

        void Normalize() { vec = vec.Normalized(); }

        [[nodiscard]] Quat Inverse() const {
            F32 lenSq = LengthSquared();
            assert(lenSq > EPSILON, "Cannot invert zero quaternion");
            return Conjugate() * (1.0f / lenSq);
        }

        [[nodiscard]] static Quat Slerp(const Quat& a, const Quat& b, F32 t) {
            Quat q1 = a.Normalized();
            Quat q2 = b.Normalized();

            F32 dot = q1.Dot(q2);

            // If the dot product is negative, negate one quaternion
            if (dot < 0.0f) {
                q2 = Quat{-q2.vec};
                dot = -dot;
            }

            // If quaternions are very close, use linear interpolation
            if (dot > 0.9995f) {
                return Quat{Vec4::Lerp(q1.vec, q2.vec, t)}.Normalized();
            }

            // Calculate the angle between quaternions
            F32 theta = std::acos(Clamp(dot, -1.0f, 1.0f));
            F32 sinTheta = std::sin(theta);

            F32 w1 = std::sin((1.0f - t) * theta) / sinTheta;
            F32 w2 = std::sin(t * theta) / sinTheta;

            return Quat{q1.vec * w1 + q2.vec * w2};
        }

        [[nodiscard]] Vec3 ToEuler() const {
            Vec3 euler;

            // Roll (x-axis rotation)
            F32 sinr_cosp = 2.0f * (w * x + y * z);
            F32 cosr_cosp = 1.0f - 2.0f * (x * x + y * y);
            euler.x = std::atan2(sinr_cosp, cosr_cosp);

            // Pitch (y-axis rotation)
            F32 sinp = 2.0f * (w * y - z * x);
            if (std::abs(sinp) >= 1.0f) {
                euler.y = std::copysign(HALF_PI, sinp);
            } else {
                euler.y = std::asin(sinp);
            }

            // Yaw (z-axis rotation)
            F32 siny_cosp = 2.0f * (w * z + x * y);
            F32 cosy_cosp = 1.0f - 2.0f * (y * y + z * z);
            euler.z = std::atan2(siny_cosp, cosy_cosp);

            return euler;
        }

        [[nodiscard]] Mat3 ToMatrix3() const {
            F32 xx = x * x;
            F32 yy = y * y;
            F32 zz = z * z;
            F32 xy = x * y;
            F32 xz = x * z;
            F32 yz = y * z;
            F32 wx = w * x;
            F32 wy = w * y;
            F32 wz = w * z;

            return Mat3{
                1.0f - 2.0f * (yy + zz), 2.0f * (xy - wz), 2.0f * (xz + wy),
                2.0f * (xy + wz), 1.0f - 2.0f * (xx + zz), 2.0f * (yz - wx),
                2.0f * (xz - wy), 2.0f * (yz + wx), 1.0f - 2.0f * (xx + yy)
            };
        }

        [[nodiscard]] Mat4 ToMatrix4() const {
            Mat3 rot = ToMatrix3();
            return Mat4{
                Vec4{rot.col0, 0.0f},
                Vec4{rot.col1, 0.0f},
                Vec4{rot.col2, 0.0f},
                Vec4{0.0f, 0.0f, 0.0f, 1.0f}
            };
        }

        [[nodiscard]] Quat operator*(F32 scalar) const { return Quat{vec * scalar}; }

        static const Quat Identity;
    };

    inline constexpr Quat Quat::Identity{0.0f, 0.0f, 0.0f, 1.0f};

    [[nodiscard]] inline Quat operator*(F32 scalar, const Quat& q) { return q * scalar; }
}
