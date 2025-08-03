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

    struct alignas(16) Mat3 {
        union {
            F32 m[3][3];  // m[row][col] for row-major storage
            F32 data[9];
            struct {
                F32 m00, m01, m02;  // row 0
                F32 m10, m11, m12;  // row 1
                F32 m20, m21, m22;  // row 2
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
            {m00, m01, m02},
            {m10, m11, m12},
            {m20, m21, m22}
        } {}

        // Column constructor for easy column-based initialization
        static constexpr Mat3 FromColumns(const Vec3& c0, const Vec3& c1, const Vec3& c2) {
            return Mat3{
                c0.x, c1.x, c2.x,
                c0.y, c1.y, c2.y,
                c0.z, c1.z, c2.z
            };
        }

        [[nodiscard]] Mat3 operator*(const Mat3& other) const {
            Mat3 result{0.0f};
            for (U32 i = 0; i < 3; ++i) {
                for (U32 j = 0; j < 3; ++j) {
                    for (U32 k = 0; k < 3; ++k) {
                        result.m[i][j] += m[i][k] * other.m[k][j];
                    }
                }
            }
            return result;
        }

        [[nodiscard]] constexpr Vec3 operator*(const Vec3& v) const {
            return Vec3{
                m[0][0] * v.x + m[0][1] * v.y + m[0][2] * v.z,
                m[1][0] * v.x + m[1][1] * v.y + m[1][2] * v.z,
                m[2][0] * v.x + m[2][1] * v.y + m[2][2] * v.z
            };
        }

        [[nodiscard]] constexpr Mat3 operator*(F32 scalar) const {
            Mat3 result;
            for (U32 i = 0; i < 3; ++i) {
                for (U32 j = 0; j < 3; ++j) {
                    result.m[i][j] = m[i][j] * scalar;
                }
            }
            return result;
        }

        [[nodiscard]] constexpr Mat3 Transposed() const {
            return {
                m[0][0], m[1][0], m[2][0],
                m[0][1], m[1][1], m[2][1],
                m[0][2], m[1][2], m[2][2]
            };
        }

        [[nodiscard]] constexpr F32 Determinant() const {
            return m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1])
                 - m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0])
                 + m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
        }

        [[nodiscard]] Mat3 Inverse() const {
            F32 det = Determinant();
            assert(std::abs(det) > EPSILON, "Matrix is singular");

            F32 invDet = 1.0f / det;
            return Mat3{
                (m[1][1] * m[2][2] - m[1][2] * m[2][1]) * invDet,
                (m[0][2] * m[2][1] - m[0][1] * m[2][2]) * invDet,
                (m[0][1] * m[1][2] - m[0][2] * m[1][1]) * invDet,
                (m[1][2] * m[2][0] - m[1][0] * m[2][2]) * invDet,
                (m[0][0] * m[2][2] - m[0][2] * m[2][0]) * invDet,
                (m[0][2] * m[1][0] - m[0][0] * m[1][2]) * invDet,
                (m[1][0] * m[2][1] - m[1][1] * m[2][0]) * invDet,
                (m[0][1] * m[2][0] - m[0][0] * m[2][1]) * invDet,
                (m[0][0] * m[1][1] - m[0][1] * m[1][0]) * invDet
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

        // Helper to get column
        [[nodiscard]] constexpr Vec3 GetColumn(U32 col) const {
            return Vec3{m[0][col], m[1][col], m[2][col]};
        }

        static const Mat3 Identity;
    };

    inline constexpr Mat3 Mat3::Identity{1.0f};

    struct alignas(16) Mat4 {
        union {
            F32 m[4][4];  // m[row][col] for row-major storage
            F32 data[16];
            __m128 rows[4];  // SIMD access by rows
        };

        constexpr Mat4() : Mat4(1.0f) {}

        constexpr explicit Mat4(F32 diagonal) : m{
            {diagonal, 0.0f, 0.0f, 0.0f},
            {0.0f, diagonal, 0.0f, 0.0f},
            {0.0f, 0.0f, diagonal, 0.0f},
            {0.0f, 0.0f, 0.0f, diagonal}
        } {}

        // Constructor from 16 values (row-major order)
        constexpr Mat4(F32 m00, F32 m01, F32 m02, F32 m03,
                      F32 m10, F32 m11, F32 m12, F32 m13,
                      F32 m20, F32 m21, F32 m22, F32 m23,
                      F32 m30, F32 m31, F32 m32, F32 m33) : m{
            {m00, m01, m02, m03},
            {m10, m11, m12, m13},
            {m20, m21, m22, m23},
            {m30, m31, m32, m33}
        } {}

        // Column-based constructor for compatibility
        static constexpr Mat4 FromColumns(const Vec4& c0, const Vec4& c1, const Vec4& c2, const Vec4& c3) {
            return Mat4{
                c0.x, c1.x, c2.x, c3.x,
                c0.y, c1.y, c2.y, c3.y,
                c0.z, c1.z, c2.z, c3.z,
                c0.w, c1.w, c2.w, c3.w
            };
        }

        Mat4(__m128 r0, __m128 r1, __m128 r2, __m128 r3)
            : rows{r0, r1, r2, r3} {}

        [[nodiscard]] inline Vec4 operator*(const Vec4& v) const {
            // Optimized matrix-vector multiplication using SIMD
            __m128 vx = _mm_set1_ps(v.x);
            __m128 vy = _mm_set1_ps(v.y);
            __m128 vz = _mm_set1_ps(v.z);
            __m128 vw = _mm_set1_ps(v.w);

            __m128 row0 = rows[0];
            __m128 row1 = rows[1];
            __m128 row2 = rows[2];
            __m128 row3 = rows[3];

            // Extract components and compute dot products
            F32 x = m[0][0] * v.x + m[0][1] * v.y + m[0][2] * v.z + m[0][3] * v.w;
            F32 y = m[1][0] * v.x + m[1][1] * v.y + m[1][2] * v.z + m[1][3] * v.w;
            F32 z = m[2][0] * v.x + m[2][1] * v.y + m[2][2] * v.z + m[2][3] * v.w;
            F32 w = m[3][0] * v.x + m[3][1] * v.y + m[3][2] * v.z + m[3][3] * v.w;

            return Vec4{x, y, z, w};
        }

        [[nodiscard]] inline Mat4 operator*(const Mat4& other) const {
            Mat4 result{0.0f};

            // Standard matrix multiplication
            for (int i = 0; i < 4; ++i) {
                for (int j = 0; j < 4; ++j) {
                    for (int k = 0; k < 4; ++k) {
                        result.m[i][j] += m[i][k] * other.m[k][j];
                    }
                }
            }

            return result;
        }

        [[nodiscard]] Mat4 operator*(F32 scalar) const {
            __m128 s = _mm_set1_ps(scalar);
            return Mat4{
                _mm_mul_ps(rows[0], s),
                _mm_mul_ps(rows[1], s),
                _mm_mul_ps(rows[2], s),
                _mm_mul_ps(rows[3], s)
            };
        }

        [[nodiscard]] Mat4 Transposed() const {
            // Efficient transpose using SIMD shuffles
            __m128 tmp0 = _mm_unpacklo_ps(rows[0], rows[1]);
            __m128 tmp2 = _mm_unpacklo_ps(rows[2], rows[3]);
            __m128 tmp1 = _mm_unpackhi_ps(rows[0], rows[1]);
            __m128 tmp3 = _mm_unpackhi_ps(rows[2], rows[3]);

            return Mat4{
                _mm_movelh_ps(tmp0, tmp2),
                _mm_movehl_ps(tmp2, tmp0),
                _mm_movelh_ps(tmp1, tmp3),
                _mm_movehl_ps(tmp3, tmp1)
            };
        }

        [[nodiscard]] Mat4 Inverse() const;

        [[nodiscard]] static Mat4 Translation(F32 x, F32 y, F32 z) {
            return Mat4{
                1.0f, 0.0f, 0.0f, x,
                0.0f, 1.0f, 0.0f, y,
                0.0f, 0.0f, 1.0f, z,
                0.0f, 0.0f, 0.0f, 1.0f
            };
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
                t * a.x * a.x + c,      t * a.x * a.y - s * a.z, t * a.x * a.z + s * a.y, 0.0f,
                t * a.x * a.y + s * a.z, t * a.y * a.y + c,      t * a.y * a.z - s * a.x, 0.0f,
                t * a.x * a.z - s * a.y, t * a.y * a.z + s * a.x, t * a.z * a.z + c,      0.0f,
                0.0f,                    0.0f,                    0.0f,                    1.0f
            };
        }

        [[nodiscard]] static Mat4 RotationX(F32 angle) {
            F32 c = std::cos(angle);
            F32 s = std::sin(angle);
            return Mat4{
                1.0f, 0.0f, 0.0f, 0.0f,
                0.0f, c,   -s,    0.0f,
                0.0f, s,    c,    0.0f,
                0.0f, 0.0f, 0.0f, 1.0f
            };
        }

        [[nodiscard]] static Mat4 RotationY(F32 angle) {
            F32 c = std::cos(angle);
            F32 s = std::sin(angle);
            return Mat4{
                c,    0.0f, s,    0.0f,
                0.0f, 1.0f, 0.0f, 0.0f,
                -s,   0.0f, c,    0.0f,
                0.0f, 0.0f, 0.0f, 1.0f
            };
        }

        [[nodiscard]] static Mat4 RotationZ(F32 angle) {
            F32 c = std::cos(angle);
            F32 s = std::sin(angle);
            return Mat4{
                c,   -s,    0.0f, 0.0f,
                s,    c,    0.0f, 0.0f,
                0.0f, 0.0f, 1.0f, 0.0f,
                0.0f, 0.0f, 0.0f, 1.0f
            };
        }

        [[nodiscard]] static Mat4 Scale(F32 sx, F32 sy, F32 sz) {
            return Mat4{
                sx,   0.0f, 0.0f, 0.0f,
                0.0f, sy,   0.0f, 0.0f,
                0.0f, 0.0f, sz,   0.0f,
                0.0f, 0.0f, 0.0f, 1.0f
            };
        }

        [[nodiscard]] static Mat4 Scale(const Vec3& scale) {
            return Scale(scale.x, scale.y, scale.z);
        }

        [[nodiscard]] static Mat4 LookAt(const Vec3& eye, const Vec3& target, const Vec3& up) {
            Vec3 f = (target - eye).Normalized();  // forward
            Vec3 r = f.Cross(up).Normalized();     // right
            Vec3 u = r.Cross(f);                    // up

            return Mat4{
                r.x,  r.y,  r.z,  -r.Dot(eye),
                u.x,  u.y,  u.z,  -u.Dot(eye),
                -f.x, -f.y, -f.z,  f.Dot(eye),
                0.0f, 0.0f, 0.0f, 1.0f
            };
        }

        [[nodiscard]] static Mat4 Perspective(F32 fovY, F32 aspect, F32 nearPlane, F32 farPlane) {
            F32 tanHalfFov = std::tan(fovY * 0.5f);
            F32 range = farPlane - nearPlane;

            return Mat4{
                1.0f / (aspect * tanHalfFov), 0.0f, 0.0f, 0.0f,
                0.0f, 1.0f / tanHalfFov, 0.0f, 0.0f,
                0.0f, 0.0f, -(farPlane + nearPlane) / range, -(2.0f * farPlane * nearPlane) / range,
                0.0f, 0.0f, -1.0f, 0.0f
            };
        }

        [[nodiscard]] static Mat4 Orthographic(F32 left, F32 right, F32 bottom, F32 top, F32 nearPlane, F32 farPlane) {
            F32 rl = right - left;
            F32 tb = top - bottom;
            F32 fn = farPlane - nearPlane;

            return Mat4{
                2.0f / rl, 0.0f, 0.0f, -(right + left) / rl,
                0.0f, 2.0f / tb, 0.0f, -(top + bottom) / tb,
                0.0f, 0.0f, -2.0f / fn, -(farPlane + nearPlane) / fn,
                0.0f, 0.0f, 0.0f, 1.0f
            };
        }

        [[nodiscard]] Vec3 GetTranslation() const {
            return Vec3{m[0][3], m[1][3], m[2][3]};
        }

        [[nodiscard]] Vec3 GetScale() const {
            return Vec3{
                Vec3{m[0][0], m[1][0], m[2][0]}.Length(),
                Vec3{m[0][1], m[1][1], m[2][1]}.Length(),
                Vec3{m[0][2], m[1][2], m[2][2]}.Length()
            };
        }

        // Helper to get column
        [[nodiscard]] Vec4 GetColumn(U32 col) const {
            return Vec4{m[0][col], m[1][col], m[2][col], m[3][col]};
        }

        void Decompose(Vec3& translation, Vec3& rotation, Vec3& scale) const;

        static const Mat4 Identity;
    };

    inline constexpr Mat4 Mat4::Identity{1.0f};

    inline Mat4 Mat4::Inverse() const {
        // Gauss-Jordan elimination optimized for 4x4 matrices
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
        translation = GetTranslation();
        scale = GetScale();

        // Extract rotation matrix
        Mat3 rotMat{
            m[0][0] / scale.x, m[0][1] / scale.y, m[0][2] / scale.z,
            m[1][0] / scale.x, m[1][1] / scale.y, m[1][2] / scale.z,
            m[2][0] / scale.x, m[2][1] / scale.y, m[2][2] / scale.z
        };

        // Extract Euler angles (Y-X-Z convention)
        rotation.x = std::atan2(rotMat.m[2][1], rotMat.m[2][2]);
        rotation.y = std::atan2(-rotMat.m[2][0], std::sqrt(rotMat.m[2][1] * rotMat.m[2][1] + rotMat.m[2][2] * rotMat.m[2][2]));
        rotation.z = std::atan2(rotMat.m[1][0], rotMat.m[0][0]);
    }
}