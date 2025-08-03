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
            // Convert to half angles
            F32 cy = std::cos(yaw * 0.5f);
            F32 sy = std::sin(yaw * 0.5f);
            F32 cp = std::cos(pitch * 0.5f);
            F32 sp = std::sin(pitch * 0.5f);
            F32 cr = std::cos(roll * 0.5f);
            F32 sr = std::sin(roll * 0.5f);

            // ZYX rotation order
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
                    (m.m[2][1] - m.m[1][2]) * s,
                    (m.m[0][2] - m.m[2][0]) * s,
                    (m.m[1][0] - m.m[0][1]) * s,
                    0.25f / s
                };
            } else if (m.m[0][0] > m.m[1][1] && m.m[0][0] > m.m[2][2]) {
                F32 s = 2.0f * std::sqrt(1.0f + m.m[0][0] - m.m[1][1] - m.m[2][2]);
                return Quat{
                    0.25f * s,
                    (m.m[0][1] + m.m[1][0]) / s,
                    (m.m[0][2] + m.m[2][0]) / s,
                    (m.m[2][1] - m.m[1][2]) / s
                };
            } else if (m.m[1][1] > m.m[2][2]) {
                F32 s = 2.0f * std::sqrt(1.0f + m.m[1][1] - m.m[0][0] - m.m[2][2]);
                return Quat{
                    (m.m[0][1] + m.m[1][0]) / s,
                    0.25f * s,
                    (m.m[1][2] + m.m[2][1]) / s,
                    (m.m[0][2] - m.m[2][0]) / s
                };
            } else {
                F32 s = 2.0f * std::sqrt(1.0f + m.m[2][2] - m.m[0][0] - m.m[1][1]);
                return Quat{
                    (m.m[0][2] + m.m[2][0]) / s,
                    (m.m[1][2] + m.m[2][1]) / s,
                    0.25f * s,
                    (m.m[1][0] - m.m[0][1]) / s
                };
            }
        }

        [[nodiscard]] static Quat LookRotation(const Vec3& forward, const Vec3& up = Vec3::Up) {
            Vec3 f = forward.Normalized();
            Vec3 r = up.Cross(f).Normalized();
            Vec3 u = f.Cross(r);

            // Build rotation matrix in row-major order
            Mat3 m{
                r.x, u.x, -f.x,
                r.y, u.y, -f.y,
                r.z, u.z, -f.z
            };

            return FromRotationMatrix(m);
        }

        [[nodiscard]] inline Quat operator*(const Quat& other) const {
            // Hamilton product: q1 * q2
            // w = w1*w2 - x1*x2 - y1*y2 - z1*z2
            // x = w1*x2 + x1*w2 + y1*z2 - z1*y2
            // y = w1*y2 - x1*z2 + y1*w2 + z1*x2
            // z = w1*z2 + x1*y2 - y1*x2 + z1*w2

            return Quat{
                w * other.x + x * other.w + y * other.z - z * other.y,
                w * other.y - x * other.z + y * other.w + z * other.x,
                w * other.z + x * other.y - y * other.x + z * other.w,
                w * other.w - x * other.x - y * other.y - z * other.z
            };
        }

        [[nodiscard]] Vec3 operator*(const Vec3& v) const {
            // Optimized rotation formula: v' = q * v * q^-1
            Vec3 qv{x, y, z};
            Vec3 t = 2.0f * qv.Cross(v);
            return v + w * t + qv.Cross(t);
        }

        [[nodiscard]] inline Quat Conjugate() const {
            return Quat{-x, -y, -z, w};
        }

        [[nodiscard]] inline F32 Dot(const Quat& other) const {
            return x * other.x + y * other.y + z * other.z + w * other.w;
        }

        [[nodiscard]] F32 Length() const {
            return std::sqrt(LengthSquared());
        }

        [[nodiscard]] F32 LengthSquared() const {
            return x * x + y * y + z * z + w * w;
        }

        [[nodiscard]] Quat Normalized() const {
            F32 len = Length();
            if (len > EPSILON) {
                F32 invLen = 1.0f / len;
                return Quat{x * invLen, y * invLen, z * invLen, w * invLen};
            }
            return Identity;
        }

        void Normalize() {
            *this = Normalized();
        }

        [[nodiscard]] Quat Inverse() const {
            F32 lenSq = LengthSquared();
            assert(lenSq > EPSILON, "Cannot invert zero quaternion");
            F32 invLenSq = 1.0f / lenSq;
            return Quat{-x * invLenSq, -y * invLenSq, -z * invLenSq, w * invLenSq};
        }

        [[nodiscard]] static Quat Slerp(const Quat& a, const Quat& b, F32 t) {
            Quat q1 = a.Normalized();
            Quat q2 = b.Normalized();

            F32 dot = q1.Dot(q2);

            // If the dot product is negative, negate one quaternion to take shorter path
            if (dot < 0.0f) {
                q2 = Quat{-q2.x, -q2.y, -q2.z, -q2.w};
                dot = -dot;
            }

            // If quaternions are very close, use linear interpolation
            if (dot > 0.9995f) {
                return Quat{
                    Lerp(q1.x, q2.x, t),
                    Lerp(q1.y, q2.y, t),
                    Lerp(q1.z, q2.z, t),
                    Lerp(q1.w, q2.w, t)
                }.Normalized();
            }

            // Calculate the angle between quaternions
            F32 theta = std::acos(Clamp(dot, -1.0f, 1.0f));
            F32 sinTheta = std::sin(theta);

            F32 w1 = std::sin((1.0f - t) * theta) / sinTheta;
            F32 w2 = std::sin(t * theta) / sinTheta;

            return Quat{
                q1.x * w1 + q2.x * w2,
                q1.y * w1 + q2.y * w2,
                q1.z * w1 + q2.z * w2,
                q1.w * w1 + q2.w * w2
            };
        }

        [[nodiscard]] Vec3 ToEuler() const {
            Vec3 euler;

            // Test for gimbal lock
            F32 test = x * y + z * w;
            if (test > 0.499f) { // singularity at north pole
                euler.z = 2.0f * std::atan2(x, w);
                euler.y = HALF_PI;
                euler.x = 0.0f;
                return euler;
            }
            if (test < -0.499f) { // singularity at south pole
                euler.z = -2.0f * std::atan2(x, w);
                euler.y = -HALF_PI;
                euler.x = 0.0f;
                return euler;
            }

            F32 sqx = x * x;
            F32 sqy = y * y;
            F32 sqz = z * z;

            euler.x = std::atan2(2.0f * (w * x + y * z), 1.0f - 2.0f * (sqx + sqy));
            euler.y = std::asin(2.0f * test);
            euler.z = std::atan2(2.0f * (w * z + x * y), 1.0f - 2.0f * (sqy + sqz));

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

            // Return matrix in row-major order
            return Mat3{
                1.0f - 2.0f * (yy + zz), 2.0f * (xy - wz),        2.0f * (xz + wy),
                2.0f * (xy + wz),        1.0f - 2.0f * (xx + zz), 2.0f * (yz - wx),
                2.0f * (xz - wy),        2.0f * (yz + wx),        1.0f - 2.0f * (xx + yy)
            };
        }

        [[nodiscard]] Mat4 ToMatrix4() const {
            F32 xx = x * x;
            F32 yy = y * y;
            F32 zz = z * z;
            F32 xy = x * y;
            F32 xz = x * z;
            F32 yz = y * z;
            F32 wx = w * x;
            F32 wy = w * y;
            F32 wz = w * z;

            // Return matrix in row-major order
            return Mat4{
                1.0f - 2.0f * (yy + zz), 2.0f * (xy - wz),        2.0f * (xz + wy),        0.0f,
                2.0f * (xy + wz),        1.0f - 2.0f * (xx + zz), 2.0f * (yz - wx),        0.0f,
                2.0f * (xz - wy),        2.0f * (yz + wx),        1.0f - 2.0f * (xx + yy), 0.0f,
                0.0f,                    0.0f,                    0.0f,                    1.0f
            };
        }

        [[nodiscard]] Quat operator*(F32 scalar) const {
            return Quat{x * scalar, y * scalar, z * scalar, w * scalar};
        }

        [[nodiscard]] Quat operator+(const Quat& other) const {
            return Quat{x + other.x, y + other.y, z + other.z, w + other.w};
        }

        [[nodiscard]] Quat operator-(const Quat& other) const {
            return Quat{x - other.x, y - other.y, z - other.z, w - other.w};
        }

        [[nodiscard]] bool operator==(const Quat& other) const {
            return std::abs(x - other.x) < EPSILON &&
                   std::abs(y - other.y) < EPSILON &&
                   std::abs(z - other.z) < EPSILON &&
                   std::abs(w - other.w) < EPSILON;
        }

        [[nodiscard]] bool operator!=(const Quat& other) const {
            return !(*this == other);
        }

        static const Quat Identity;
    };

    inline constexpr Quat Quat::Identity{0.0f, 0.0f, 0.0f, 1.0f};

    [[nodiscard]] inline Quat operator*(F32 scalar, const Quat& q) {
        return q * scalar;
    }
}