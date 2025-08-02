module;
#include <immintrin.h>
#include <cmath>

export module Math.Matrix;

import Core.Types;
import Core.Assert;
import Math.Core;
import Math.Vector;
import std;

export namespace Math {
    // Column-major 3x3 matrix
    struct alignas(16) Mat3 {
        union {
            F32 m[3][3];
            F32 data[9];
            struct {
                Vec3 col0, col1, col2;
            };
        };

        constexpr Mat3() : Mat3(1.0f) {}

        constexpr explicit Mat3(F32 diagonal) : m{
            {diagonal, 0.0f, 0.0f},
            {0.0f, diagonal, 0.0f},
            {0.0f, 0.0f, diagonal}
        } {}

        constexpr Mat3(F32 m00, F32 m01, F32 m02,
                      F32 m10, F32 m11, F32 m12,
                      F32 m20, F32 m21, F32 m22) : m{
            {m00, m10, m20},
            {m01, m11, m21},
            {m02, m12, m22}
        } {}

        constexpr Mat3(const Vec3& c0, const Vec3& c1, const Vec3& c2)
            : col0{c0}, col1{c1}, col2{c2} {}

        [[nodiscard]] constexpr Vec3 operator*(const Vec3& v) const {
            return {
                m[0][0] * v.x + m[1][0] * v.y + m[2][0] * v.z,
                m[0][1] * v.x + m[1][1] * v.y + m[2][1] * v.z,
                m[0][2] * v.x + m[1][2] * v.y + m[2][2] * v.z
            };
        }

        [[nodiscard]] Mat3 operator*(const Mat3& other) const {
            Mat3 result{0.0f};
            for (U32 i = 0; i < 3; ++i) {
                for (U32 j = 0; j < 3; ++j) {
                    for (U32 k = 0; k < 3; ++k) {
                        result.m[i][j] += m[k][j] * other.m[i][k];
                    }
                }
            }
            return result;
        }

        [[nodiscard]] constexpr Mat3 operator*(F32 scalar) const {
            return {
                col0 * scalar,
                col1 * scalar,
                col2 * scalar
            };
        }

        [[nodiscard]] constexpr Mat3 Transposed() const {
            return {
                m[0][0], m[0][1], m[0][2],
                m[1][0], m[1][1], m[1][2],
                m[2][0], m[2][1], m[2][2]
            };
        }

        [[nodiscard]] constexpr F32 Determinant() const {
            return m[0][0] * (m[1][1] * m[2][2] - m[2][1] * m[1][2])
                 - m[1][0] * (m[0][1] * m[2][2] - m[2][1] * m[0][2])
                 + m[2][0] * (m[0][1] * m[1][2] - m[1][1] * m[0][2]);
        }

        [[nodiscard]] Mat3 Inverse() const {
            F32 det = Determinant();
            assert(std::abs(det) > EPSILON, "Matrix is singular");

            F32 invDet = 1.0f / det;
            return Mat3{
                (m[1][1] * m[2][2] - m[2][1] * m[1][2]) * invDet,
                (m[2][1] * m[0][2] - m[0][1] * m[2][2]) * invDet,
                (m[0][1] * m[1][2] - m[1][1] * m[0][2]) * invDet,
                (m[2][0] * m[1][2] - m[1][0] * m[2][2]) * invDet,
                (m[0][0] * m[2][2] - m[2][0] * m[0][2]) * invDet,
                (m[1][0] * m[0][2] - m[0][0] * m[1][2]) * invDet,
                (m[1][0] * m[2][1] - m[2][0] * m[1][1]) * invDet,
                (m[2][0] * m[0][1] - m[0][0] * m[2][1]) * invDet,
                (m[0][0] * m[1][1] - m[1][0] * m[0][1]) * invDet
            };
        }

        [[nodiscard]] static Mat3 Rotation(F32 angle) {
            F32 c = std::cos(angle);
            F32 s = std::sin(angle);
            return {
                c, -s, 0.0f,
                s,  c, 0.0f,
                0.0f, 0.0f, 1.0f
            };
        }

        [[nodiscard]] static constexpr Mat3 Scale(F32 sx, F32 sy) {
            return {
                sx, 0.0f, 0.0f,
                0.0f, sy, 0.0f,
                0.0f, 0.0f, 1.0f
            };
        }

        [[nodiscard]] static constexpr Mat3 Scale(const Vec2& scale) {
            return Scale(scale.x, scale.y);
        }

        static const Mat3 Identity;
    };

    inline constexpr Mat3 Mat3::Identity{1.0f};

    // Column-major 4x4 matrix
    struct alignas(16) Mat4 {
        union {
            F32 m[4][4];
            F32 data[16];
            struct {
                Vec4 col0, col1, col2, col3;
            };
            __m128 columns[4];
        };

        constexpr Mat4() : Mat4(1.0f) {}

        constexpr explicit Mat4(F32 diagonal) : m{
            {diagonal, 0.0f, 0.0f, 0.0f},
            {0.0f, diagonal, 0.0f, 0.0f},
            {0.0f, 0.0f, diagonal, 0.0f},
            {0.0f, 0.0f, 0.0f, diagonal}
        } {}

        constexpr Mat4(const Vec4& c0, const Vec4& c1, const Vec4& c2, const Vec4& c3)
            : col0{c0}, col1{c1}, col2{c2}, col3{c3} {}

        Mat4(__m128 c0, __m128 c1, __m128 c2, __m128 c3)
            : columns{c0, c1, c2, c3} {}

        [[nodiscard]] inline Vec4 operator*(const Vec4& v) const {
            __m128 vx = _mm_set1_ps(v.x);
            __m128 vy = _mm_set1_ps(v.y);
            __m128 vz = _mm_set1_ps(v.z);
            __m128 vw = _mm_set1_ps(v.w);

            __m128 result = _mm_mul_ps(columns[0], vx);
            result = _mm_add_ps(result, _mm_mul_ps(columns[1], vy));
            result = _mm_add_ps(result, _mm_mul_ps(columns[2], vz));
            result = _mm_add_ps(result, _mm_mul_ps(columns[3], vw));

            return Vec4{result};
        }

        [[nodiscard]] inline Mat4 operator*(const Mat4& other) const {
            Mat4 result;
            for (int i = 0; i < 4; ++i) {
                __m128 c0 = _mm_set1_ps(other.m[i][0]);
                __m128 c1 = _mm_set1_ps(other.m[i][1]);
                __m128 c2 = _mm_set1_ps(other.m[i][2]);
                __m128 c3 = _mm_set1_ps(other.m[i][3]);

                result.columns[i] = _mm_add_ps(
                    _mm_add_ps(_mm_mul_ps(columns[0], c0), _mm_mul_ps(columns[1], c1)),
                    _mm_add_ps(_mm_mul_ps(columns[2], c2), _mm_mul_ps(columns[3], c3))
                );
            }
            return result;
        }

        [[nodiscard]] Mat4 operator*(F32 scalar) const {
            __m128 s = _mm_set1_ps(scalar);
            return Mat4{
                _mm_mul_ps(columns[0], s),
                _mm_mul_ps(columns[1], s),
                _mm_mul_ps(columns[2], s),
                _mm_mul_ps(columns[3], s)
            };
        }

        [[nodiscard]] Mat4 Transposed() const {
            __m128 tmp0 = _mm_unpacklo_ps(columns[0], columns[1]);
            __m128 tmp2 = _mm_unpacklo_ps(columns[2], columns[3]);
            __m128 tmp1 = _mm_unpackhi_ps(columns[0], columns[1]);
            __m128 tmp3 = _mm_unpackhi_ps(columns[2], columns[3]);

            return Mat4{
                _mm_movelh_ps(tmp0, tmp2),
                _mm_movehl_ps(tmp2, tmp0),
                _mm_movelh_ps(tmp1, tmp3),
                _mm_movehl_ps(tmp3, tmp1)
            };
        }

        [[nodiscard]] Mat4 Inverse() const;

        [[nodiscard]] static Mat4 Translation(F32 x, F32 y, F32 z) {
            Mat4 result{1.0f};
            result.m[3][0] = x;
            result.m[3][1] = y;
            result.m[3][2] = z;
            return result;
        }

        [[nodiscard]] static Mat4 Translation(const Vec3& translation) {
            return Translation(translation.x, translation.y, translation.z);
        }

        [[nodiscard]] static Mat4 Rotation(const Vec3& axis, F32 angle) {
            F32 c = std::cos(angle);
            F32 s = std::sin(angle);
            F32 t = 1.0f - c;
            Vec3 a = axis.Normalized();

            return Mat4{
                Vec4{t * a.x * a.x + c, t * a.x * a.y + s * a.z, t * a.x * a.z - s * a.y, 0.0f},
                Vec4{t * a.x * a.y - s * a.z, t * a.y * a.y + c, t * a.y * a.z + s * a.x, 0.0f},
                Vec4{t * a.x * a.z + s * a.y, t * a.y * a.z - s * a.x, t * a.z * a.z + c, 0.0f},
                Vec4{0.0f, 0.0f, 0.0f, 1.0f}
            };
        }

        [[nodiscard]] static Mat4 RotationX(F32 angle) {
            F32 c = std::cos(angle);
            F32 s = std::sin(angle);
            return Mat4{
                Vec4{1.0f, 0.0f, 0.0f, 0.0f},
                Vec4{0.0f, c, s, 0.0f},
                Vec4{0.0f, -s, c, 0.0f},
                Vec4{0.0f, 0.0f, 0.0f, 1.0f}
            };
        }

        [[nodiscard]] static Mat4 RotationY(F32 angle) {
            F32 c = std::cos(angle);
            F32 s = std::sin(angle);
            return Mat4{
                Vec4{c, 0.0f, -s, 0.0f},
                Vec4{0.0f, 1.0f, 0.0f, 0.0f},
                Vec4{s, 0.0f, c, 0.0f},
                Vec4{0.0f, 0.0f, 0.0f, 1.0f}
            };
        }

        [[nodiscard]] static Mat4 RotationZ(F32 angle) {
            F32 c = std::cos(angle);
            F32 s = std::sin(angle);
            return Mat4{
                Vec4{c, s, 0.0f, 0.0f},
                Vec4{-s, c, 0.0f, 0.0f},
                Vec4{0.0f, 0.0f, 1.0f, 0.0f},
                Vec4{0.0f, 0.0f, 0.0f, 1.0f}
            };
        }

        [[nodiscard]] static Mat4 Scale(F32 sx, F32 sy, F32 sz) {
            return Mat4{
                Vec4{sx, 0.0f, 0.0f, 0.0f},
                Vec4{0.0f, sy, 0.0f, 0.0f},
                Vec4{0.0f, 0.0f, sz, 0.0f},
                Vec4{0.0f, 0.0f, 0.0f, 1.0f}
            };
        }

        [[nodiscard]] static Mat4 Scale(const Vec3& scale) {
            return Scale(scale.x, scale.y, scale.z);
        }

        [[nodiscard]] static Mat4 LookAt(const Vec3& eye, const Vec3& target, const Vec3& up) {
            Vec3 f = (target - eye).Normalized();
            Vec3 r = f.Cross(up).Normalized();
            Vec3 u = r.Cross(f);

            return Mat4{
                Vec4{r.x, u.x, -f.x, 0.0f},
                Vec4{r.y, u.y, -f.y, 0.0f},
                Vec4{r.z, u.z, -f.z, 0.0f},
                Vec4{-r.Dot(eye), -u.Dot(eye), f.Dot(eye), 1.0f}
            };
        }

        [[nodiscard]] static Mat4 Perspective(F32 fovY, F32 aspect, F32 nearPlane, F32 farPlane) {
            F32 tanHalfFov = std::tan(fovY * 0.5f);
            Mat4 result{0.0f};
            result.m[0][0] = 1.0f / (aspect * tanHalfFov);
            result.m[1][1] = 1.0f / tanHalfFov;
            result.m[2][2] = -(farPlane + nearPlane) / (farPlane - nearPlane);
            result.m[2][3] = -1.0f;
            result.m[3][2] = -(2.0f * farPlane * nearPlane) / (farPlane - nearPlane);
            return result;
        }

        [[nodiscard]] static Mat4 Orthographic(F32 left, F32 right, F32 bottom, F32 top, F32 nearPlane, F32 farPlane) {
            Mat4 result{1.0f};
            result.m[0][0] = 2.0f / (right - left);
            result.m[1][1] = 2.0f / (top - bottom);
            result.m[2][2] = -2.0f / (farPlane - nearPlane);
            result.m[3][0] = -(right + left) / (right - left);
            result.m[3][1] = -(top + bottom) / (top - bottom);
            result.m[3][2] = -(farPlane + nearPlane) / (farPlane - nearPlane);
            return result;
        }

        [[nodiscard]] Vec3 GetTranslation() const {
            return Vec3{m[3][0], m[3][1], m[3][2]};
        }

        [[nodiscard]] Vec3 GetScale() const {
            return Vec3{
                Vec3{m[0][0], m[0][1], m[0][2]}.Length(),
                Vec3{m[1][0], m[1][1], m[1][2]}.Length(),
                Vec3{m[2][0], m[2][1], m[2][2]}.Length()
            };
        }

        void Decompose(Vec3& translation, Vec3& rotation, Vec3& scale) const;

        static const Mat4 Identity;
    };

    inline constexpr Mat4 Mat4::Identity{1.0f};

    // Mat4 Inverse implementation
    inline Mat4 Mat4::Inverse() const {
        // Compute adjugate matrix
        F32 a00 = m[0][0], a01 = m[0][1], a02 = m[0][2], a03 = m[0][3];
        F32 a10 = m[1][0], a11 = m[1][1], a12 = m[1][2], a13 = m[1][3];
        F32 a20 = m[2][0], a21 = m[2][1], a22 = m[2][2], a23 = m[2][3];
        F32 a30 = m[3][0], a31 = m[3][1], a32 = m[3][2], a33 = m[3][3];

        F32 b00 = a00 * a11 - a01 * a10;
        F32 b01 = a00 * a12 - a02 * a10;
        F32 b02 = a00 * a13 - a03 * a10;
        F32 b03 = a01 * a12 - a02 * a11;
        F32 b04 = a01 * a13 - a03 * a11;
        F32 b05 = a02 * a13 - a03 * a12;
        F32 b06 = a20 * a31 - a21 * a30;
        F32 b07 = a20 * a32 - a22 * a30;
        F32 b08 = a20 * a33 - a23 * a30;
        F32 b09 = a21 * a32 - a22 * a31;
        F32 b10 = a21 * a33 - a23 * a31;
        F32 b11 = a22 * a33 - a23 * a32;

        F32 det = b00 * b11 - b01 * b10 + b02 * b09 + b03 * b08 - b04 * b07 + b05 * b06;
        assert(std::abs(det) > EPSILON, "Matrix is singular");

        F32 invDet = 1.0f / det;

        Mat4 result;
        result.m[0][0] = (a11 * b11 - a12 * b10 + a13 * b09) * invDet;
        result.m[0][1] = (a02 * b10 - a01 * b11 - a03 * b09) * invDet;
        result.m[0][2] = (a31 * b05 - a32 * b04 + a33 * b03) * invDet;
        result.m[0][3] = (a22 * b04 - a21 * b05 - a23 * b03) * invDet;
        result.m[1][0] = (a12 * b08 - a10 * b11 - a13 * b07) * invDet;
        result.m[1][1] = (a00 * b11 - a02 * b08 + a03 * b07) * invDet;
        result.m[1][2] = (a32 * b02 - a30 * b05 - a33 * b01) * invDet;
        result.m[1][3] = (a20 * b05 - a22 * b02 + a23 * b01) * invDet;
        result.m[2][0] = (a10 * b10 - a11 * b08 + a13 * b06) * invDet;
        result.m[2][1] = (a01 * b08 - a00 * b10 - a03 * b06) * invDet;
        result.m[2][2] = (a30 * b04 - a31 * b02 + a33 * b00) * invDet;
        result.m[2][3] = (a21 * b02 - a20 * b04 - a23 * b00) * invDet;
        result.m[3][0] = (a11 * b07 - a10 * b09 - a12 * b06) * invDet;
        result.m[3][1] = (a00 * b09 - a01 * b07 + a02 * b06) * invDet;
        result.m[3][2] = (a31 * b01 - a30 * b03 - a32 * b00) * invDet;
        result.m[3][3] = (a20 * b03 - a21 * b01 + a22 * b00) * invDet;

        return result;
    }

    inline void Mat4::Decompose(Vec3& translation, Vec3& rotation, Vec3& scale) const {
        // Extract translation
        translation = GetTranslation();

        // Extract scale
        scale = GetScale();

        // Extract rotation matrix
        Mat3 rotMat{
            m[0][0] / scale.x, m[0][1] / scale.x, m[0][2] / scale.x,
            m[1][0] / scale.y, m[1][1] / scale.y, m[1][2] / scale.y,
            m[2][0] / scale.z, m[2][1] / scale.z, m[2][2] / scale.z
        };

        // Convert to Euler angles (Y-X-Z order)
        rotation.x = std::atan2(rotMat.m[2][1], rotMat.m[2][2]);
        rotation.y = std::atan2(-rotMat.m[2][0], std::sqrt(rotMat.m[2][1] * rotMat.m[2][1] + rotMat.m[2][2] * rotMat.m[2][2]));
        rotation.z = std::atan2(rotMat.m[1][0], rotMat.m[0][0]);
    }
}