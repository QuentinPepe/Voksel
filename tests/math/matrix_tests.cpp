#include <catch2/catch.hpp>
#include <cmath>

import Core.Types;
import Math.Matrix;
import Math.Vector;
import Math.Core;

using namespace Math;

template<typename T>
bool MatrixAlmostEqual(const T& a, const T& b, F32 epsilon = EPSILON) {
    for (int i = 0; i < std::size(a.data); ++i) {
        if (!AlmostEqual(a.data[i], b.data[i], epsilon)) {
            return false;
        }
    }
    return true;
}

TEST_CASE("Mat3 Construction", "[Matrix]") {
    SECTION("Default constructor creates identity matrix") {
        Mat3 m;
        REQUIRE(m.m[0][0] == 1.0f);
        REQUIRE(m.m[1][1] == 1.0f);
        REQUIRE(m.m[2][2] == 1.0f);
        REQUIRE(m.m[0][1] == 0.0f);
        REQUIRE(m.m[1][0] == 0.0f);
    }

    SECTION("Diagonal constructor") {
        Mat3 m{2.0f};
        REQUIRE(m.m[0][0] == 2.0f);
        REQUIRE(m.m[1][1] == 2.0f);
        REQUIRE(m.m[2][2] == 2.0f);
        REQUIRE(m.m[0][1] == 0.0f);
    }

    SECTION("Element constructor") {
        Mat3 m{1.0f, 2.0f, 3.0f,
               4.0f, 5.0f, 6.0f,
               7.0f, 8.0f, 9.0f};
        REQUIRE(m.m[0][0] == 1.0f);
        REQUIRE(m.m[0][1] == 2.0f);
        REQUIRE(m.m[0][2] == 3.0f);
        REQUIRE(m.m[1][0] == 4.0f);
    }

    SECTION("Column vector constructor using FromColumns") {
        Vec3 c0{1.0f, 2.0f, 3.0f};
        Vec3 c1{4.0f, 5.0f, 6.0f};
        Vec3 c2{7.0f, 8.0f, 9.0f};
        Mat3 m = Mat3::FromColumns(c0, c1, c2);
        // Verify the columns are correctly stored
        REQUIRE(m.GetColumn(0) == c0);
        REQUIRE(m.GetColumn(1) == c1);
        REQUIRE(m.GetColumn(2) == c2);
    }
}

TEST_CASE("Mat3 Vector multiplication", "[Matrix]") {
    Mat3 m{2.0f, 0.0f, 0.0f,
           0.0f, 3.0f, 0.0f,
           0.0f, 0.0f, 4.0f};
    Vec3 v{1.0f, 2.0f, 3.0f};
    Vec3 result = m * v;
    REQUIRE(result.x == 2.0f);
    REQUIRE(result.y == 6.0f);
    REQUIRE(result.z == 12.0f);
}

TEST_CASE("Mat3 Matrix multiplication", "[Matrix]") {
    Mat3 m1{1.0f, 2.0f, 3.0f,
            4.0f, 5.0f, 6.0f,
            7.0f, 8.0f, 9.0f};
    Mat3 m2{9.0f, 8.0f, 7.0f,
            6.0f, 5.0f, 4.0f,
            3.0f, 2.0f, 1.0f};
    Mat3 result = m1 * m2;

    // Expected result for row-major multiplication
    REQUIRE(result.m[0][0] == 30.0f);
    REQUIRE(result.m[0][1] == 24.0f);
    REQUIRE(result.m[0][2] == 18.0f);
    REQUIRE(result.m[1][0] == 84.0f);
    REQUIRE(result.m[1][1] == 69.0f);
    REQUIRE(result.m[1][2] == 54.0f);
    REQUIRE(result.m[2][0] == 138.0f);
    REQUIRE(result.m[2][1] == 114.0f);
    REQUIRE(result.m[2][2] == 90.0f);
}

TEST_CASE("Mat3 Scalar multiplication", "[Matrix]") {
    Mat3 m{1.0f, 2.0f, 3.0f,
           4.0f, 5.0f, 6.0f,
           7.0f, 8.0f, 9.0f};
    Mat3 result = m * 2.0f;
    REQUIRE(result.m[0][0] == 2.0f);
    REQUIRE(result.m[1][1] == 10.0f);
    REQUIRE(result.m[2][2] == 18.0f);
}

TEST_CASE("Mat3 Transpose", "[Matrix]") {
    Mat3 m{1.0f, 2.0f, 3.0f,
           4.0f, 5.0f, 6.0f,
           7.0f, 8.0f, 9.0f};
    Mat3 result = m.Transposed();
    REQUIRE(result.m[0][0] == 1.0f);
    REQUIRE(result.m[0][1] == 4.0f);
    REQUIRE(result.m[0][2] == 7.0f);
    REQUIRE(result.m[1][0] == 2.0f);
}

TEST_CASE("Mat3 Determinant", "[Matrix]") {
    SECTION("Identity matrix") {
        Mat3 m{1.0f};
        REQUIRE(AlmostEqual(m.Determinant(), 1.0f));
    }

    SECTION("Scaling matrix") {
        Mat3 m = Mat3::Scale(2.0f, 3.0f);
        REQUIRE(AlmostEqual(m.Determinant(), 6.0f));
    }

    SECTION("Singular matrix") {
        Mat3 m{1.0f, 2.0f, 3.0f,
               4.0f, 5.0f, 6.0f,
               7.0f, 8.0f, 9.0f};
        REQUIRE(AlmostEqual(m.Determinant(), 0.0f));
    }
}

TEST_CASE("Mat3 Inverse", "[Matrix]") {
    SECTION("Identity matrix inverse is identity") {
        Mat3 m{1.0f};
        Mat3 inv = m.Inverse();
        Mat3 idt = Mat3::Identity;
        REQUIRE(MatrixAlmostEqual(inv, Mat3::Identity));
    }

    SECTION("Inverse of scaling matrix") {
        Mat3 m = Mat3::Scale(2.0f, 4.0f);
        Mat3 inv = m.Inverse();
        Mat3 result = m * inv;
        REQUIRE(MatrixAlmostEqual(result, Mat3::Identity));
    }

    SECTION("Inverse of rotation matrix") {
        Mat3 m = Mat3::Rotation(PI / 4.0f);
        Mat3 inv = m.Inverse();
        Mat3 result = m * inv;
        REQUIRE(MatrixAlmostEqual(result, Mat3::Identity, 1e-5f));
    }

    SECTION("General matrix inverse") {
        Mat3 m{2.0f, 1.0f, 0.0f,
               1.0f, 2.0f, 1.0f,
               0.0f, 1.0f, 2.0f};
        Mat3 inv = m.Inverse();
        Mat3 result = m * inv;
        REQUIRE(MatrixAlmostEqual(result, Mat3::Identity, 1e-5f));
    }
}

TEST_CASE("Mat3 Static methods", "[Matrix]") {
    SECTION("Rotation matrix") {
        Mat3 rot = Mat3::Rotation(PI / 2.0f);
        Vec3 v{1.0f, 0.0f, 0.0f};
        Vec3 result = rot * v;
        REQUIRE(AlmostEqual(result.x, 0.0f, 1e-6f));
        REQUIRE(AlmostEqual(result.y, 1.0f, 1e-6f));
        REQUIRE(AlmostEqual(result.z, 0.0f));
    }

    SECTION("Scale matrix with scalars") {
        Mat3 scale = Mat3::Scale(2.0f, 3.0f);
        Vec3 v{1.0f, 1.0f, 1.0f};
        Vec3 result = scale * v;
        REQUIRE(result.x == 2.0f);
        REQUIRE(result.y == 3.0f);
        REQUIRE(result.z == 1.0f);
    }

    SECTION("Scale matrix with Vec2") {
        Vec2 scaleVec{2.0f, 3.0f};
        Mat3 scale = Mat3::Scale(scaleVec);
        Vec3 v{1.0f, 1.0f, 1.0f};
        Vec3 result = scale * v;
        REQUIRE(result.x == 2.0f);
        REQUIRE(result.y == 3.0f);
        REQUIRE(result.z == 1.0f);
    }
}

TEST_CASE("Mat4 Construction", "[Matrix]") {
    SECTION("Default constructor creates identity matrix") {
        Mat4 m;
        REQUIRE(m.m[0][0] == 1.0f);
        REQUIRE(m.m[1][1] == 1.0f);
        REQUIRE(m.m[2][2] == 1.0f);
        REQUIRE(m.m[3][3] == 1.0f);
        REQUIRE(m.m[0][1] == 0.0f);
    }

    SECTION("Diagonal constructor") {
        Mat4 m{3.0f};
        REQUIRE(m.m[0][0] == 3.0f);
        REQUIRE(m.m[1][1] == 3.0f);
        REQUIRE(m.m[2][2] == 3.0f);
        REQUIRE(m.m[3][3] == 3.0f);
    }

    SECTION("Column vector constructor using FromColumns") {
        Vec4 c0{1.0f, 2.0f, 3.0f, 4.0f};
        Vec4 c1{5.0f, 6.0f, 7.0f, 8.0f};
        Vec4 c2{9.0f, 10.0f, 11.0f, 12.0f};
        Vec4 c3{13.0f, 14.0f, 15.0f, 16.0f};
        Mat4 m = Mat4::FromColumns(c0, c1, c2, c3);
        REQUIRE(m.GetColumn(0) == c0);
        REQUIRE(m.GetColumn(1) == c1);
        REQUIRE(m.GetColumn(2) == c2);
        REQUIRE(m.GetColumn(3) == c3);
    }
}

TEST_CASE("Mat4 Vector multiplication", "[Matrix]") {
    Mat4 m = Mat4::Scale(2.0f, 3.0f, 4.0f);
    Vec4 v{1.0f, 2.0f, 3.0f, 1.0f};
    Vec4 result = m * v;
    REQUIRE(result.x == 2.0f);
    REQUIRE(result.y == 6.0f);
    REQUIRE(result.z == 12.0f);
    REQUIRE(result.w == 1.0f);
}

TEST_CASE("Mat4 Matrix multiplication", "[Matrix]") {
    SECTION("Translation * Scale") {
        Mat4 m1 = Mat4::Translation(1.0f, 2.0f, 3.0f);
        Mat4 m2 = Mat4::Scale(2.0f, 2.0f, 2.0f);
        Mat4 result = m1 * m2;

        // Translation should remain the same
        REQUIRE(result.m[0][3] == 1.0f);
        REQUIRE(result.m[1][3] == 2.0f);
        REQUIRE(result.m[2][3] == 3.0f);
        // Scale should be applied
        REQUIRE(result.m[0][0] == 2.0f);
        REQUIRE(result.m[1][1] == 2.0f);
        REQUIRE(result.m[2][2] == 2.0f);
    }

    SECTION("Scale * Translation") {
        Mat4 m1 = Mat4::Scale(2.0f, 2.0f, 2.0f);
        Mat4 m2 = Mat4::Translation(1.0f, 2.0f, 3.0f);
        Mat4 result = m1 * m2;

        // Translation should be scaled
        REQUIRE(result.m[0][3] == 2.0f);
        REQUIRE(result.m[1][3] == 4.0f);
        REQUIRE(result.m[2][3] == 6.0f);
    }
}

TEST_CASE("Mat4 Scalar multiplication", "[Matrix]") {
    Mat4 m = Mat4::Identity;
    Mat4 result = m * 3.0f;
    REQUIRE(result.m[0][0] == 3.0f);
    REQUIRE(result.m[1][1] == 3.0f);
    REQUIRE(result.m[2][2] == 3.0f);
    REQUIRE(result.m[3][3] == 3.0f);
}

TEST_CASE("Mat4 Transpose", "[Matrix]") {
    Mat4 m = Mat4::Translation(1.0f, 2.0f, 3.0f);
    Mat4 result = m.Transposed();
    REQUIRE(result.m[3][0] == 1.0f);
    REQUIRE(result.m[3][1] == 2.0f);
    REQUIRE(result.m[3][2] == 3.0f);
}

TEST_CASE("Mat4 Inverse", "[Matrix]") {
    SECTION("Identity inverse") {
        Mat4 m = Mat4::Identity;
        Mat4 inv = m.Inverse();
        REQUIRE(MatrixAlmostEqual(inv, Mat4::Identity));
    }

    SECTION("Translation inverse") {
        Mat4 m = Mat4::Translation(5.0f, -3.0f, 2.0f);
        Mat4 inv = m.Inverse();
        Mat4 result = m * inv;
        REQUIRE(MatrixAlmostEqual(result, Mat4::Identity, 1e-5f));
    }

    SECTION("Combined transform inverse") {
        Mat4 m = Mat4::Translation(1.0f, 2.0f, 3.0f) * Mat4::Scale(2.0f, 2.0f, 2.0f);
        Mat4 inv = m.Inverse();
        Mat4 result = m * inv;
        REQUIRE(MatrixAlmostEqual(result, Mat4::Identity, 1e-5f));
    }

    SECTION("Rotation inverse") {
        Mat4 m = Mat4::RotationY(PI / 3.0f);
        Mat4 inv = m.Inverse();
        Mat4 result = m * inv;
        REQUIRE(MatrixAlmostEqual(result, Mat4::Identity, 1e-5f));
    }
}

TEST_CASE("Mat4 Translation", "[Matrix]") {
    SECTION("Translation with scalars") {
        Mat4 m = Mat4::Translation(1.0f, 2.0f, 3.0f);
        Vec4 v{0.0f, 0.0f, 0.0f, 1.0f};
        Vec4 result = m * v;
        REQUIRE(result.x == 1.0f);
        REQUIRE(result.y == 2.0f);
        REQUIRE(result.z == 3.0f);
        REQUIRE(result.w == 1.0f);
    }

    SECTION("Translation with Vec3") {
        Vec3 trans{4.0f, 5.0f, 6.0f};
        Mat4 m = Mat4::Translation(trans);
        Vec4 v{0.0f, 0.0f, 0.0f, 1.0f};
        Vec4 result = m * v;
        REQUIRE(result.x == 4.0f);
        REQUIRE(result.y == 5.0f);
        REQUIRE(result.z == 6.0f);
    }

    SECTION("GetTranslation") {
        Vec3 trans{5.0f, -3.0f, 2.0f};
        Mat4 m = Mat4::Translation(trans);
        Vec3 result = m.GetTranslation();
        REQUIRE(result == trans);
    }
}

TEST_CASE("Mat4 Rotation", "[Matrix]") {
    SECTION("Rotation around arbitrary axis") {
        Vec3 axis = Vec3::UnitY;
        Mat4 m = Mat4::Rotation(axis, PI / 2.0f);
        Vec4 v{1.0f, 0.0f, 0.0f, 1.0f};
        Vec4 result = m * v;
        REQUIRE(AlmostEqual(result.x, 0.0f, 1e-6f));
        REQUIRE(AlmostEqual(result.y, 0.0f, 1e-6f));
        REQUIRE(AlmostEqual(result.z, -1.0f, 1e-6f));
    }

    SECTION("RotationX") {
        Mat4 m = Mat4::RotationX(PI / 2.0f);
        Vec4 v{0.0f, 1.0f, 0.0f, 1.0f};
        Vec4 result = m * v;
        REQUIRE(AlmostEqual(result.x, 0.0f));
        REQUIRE(AlmostEqual(result.y, 0.0f, 1e-6f));
        REQUIRE(AlmostEqual(result.z, 1.0f, 1e-6f));
    }

    SECTION("RotationY") {
        Mat4 m = Mat4::RotationY(PI / 2.0f);
        Vec4 v{1.0f, 0.0f, 0.0f, 1.0f};
        Vec4 result = m * v;
        REQUIRE(AlmostEqual(result.x, 0.0f, 1e-6f));
        REQUIRE(AlmostEqual(result.y, 0.0f));
        REQUIRE(AlmostEqual(result.z, -1.0f, 1e-6f));
    }

    SECTION("RotationZ") {
        Mat4 m = Mat4::RotationZ(PI / 2.0f);
        Vec4 v{1.0f, 0.0f, 0.0f, 1.0f};
        Vec4 result = m * v;
        REQUIRE(AlmostEqual(result.x, 0.0f, 1e-6f));
        REQUIRE(AlmostEqual(result.y, 1.0f, 1e-6f));
        REQUIRE(AlmostEqual(result.z, 0.0f));
    }
}

TEST_CASE("Mat4 Scale", "[Matrix]") {
    SECTION("Scale with scalars") {
        Mat4 m = Mat4::Scale(2.0f, 3.0f, 4.0f);
        Vec4 v{1.0f, 1.0f, 1.0f, 1.0f};
        Vec4 result = m * v;
        REQUIRE(result.x == 2.0f);
        REQUIRE(result.y == 3.0f);
        REQUIRE(result.z == 4.0f);
        REQUIRE(result.w == 1.0f);
    }

    SECTION("Scale with Vec3") {
        Vec3 scale{2.0f, 3.0f, 4.0f};
        Mat4 m = Mat4::Scale(scale);
        Vec4 v{1.0f, 1.0f, 1.0f, 1.0f};
        Vec4 result = m * v;
        REQUIRE(result.x == 2.0f);
        REQUIRE(result.y == 3.0f);
        REQUIRE(result.z == 4.0f);
    }

    SECTION("GetScale") {
        Vec3 scale{2.0f, 3.0f, 4.0f};
        Mat4 m = Mat4::Scale(scale);
        Vec3 result = m.GetScale();
        REQUIRE(AlmostEqual(result.x, scale.x));
        REQUIRE(AlmostEqual(result.y, scale.y));
        REQUIRE(AlmostEqual(result.z, scale.z));
    }
}

TEST_CASE("Mat4 LookAt", "[Matrix]") {
    Vec3 eye{0.0f, 0.0f, 5.0f};
    Vec3 target{0.0f, 0.0f, 0.0f};
    Vec3 up{0.0f, 1.0f, 0.0f};
    Mat4 m = Mat4::LookAt(eye, target, up);

    Vec4 targetPoint{0.0f, 0.0f, 0.0f, 1.0f};
    Vec4 result = m * targetPoint;
    REQUIRE(AlmostEqual(result.x, 0.0f, 1e-6f));
    REQUIRE(AlmostEqual(result.y, 0.0f, 1e-6f));
    REQUIRE(AlmostEqual(result.z, -5.0f, 1e-6f));
}

TEST_CASE("Mat4 Perspective", "[Matrix]") {
    F32 fov = PI / 4.0f;
    F32 aspect = 16.0f / 9.0f;
    F32 nearV = 0.1f;
    F32 farV = 100.0f;
    Mat4 m = Mat4::Perspective(fov, aspect, nearV, farV);

    Vec4 nearCenter{0.0f, 0.0f, -nearV, 1.0f};
    Vec4 result = m * nearCenter;
    REQUIRE(AlmostEqual(result.z / result.w, -1.0f, 1e-5f));
}

TEST_CASE("Mat4 Orthographic", "[Matrix]") {
    Mat4 m = Mat4::Orthographic(-1.0f, 1.0f, -1.0f, 1.0f, 0.1f, 100.0f);

    Vec4 rightEdge{1.0f, 0.0f, -1.0f, 1.0f};
    Vec4 result = m * rightEdge;
    REQUIRE(AlmostEqual(result.x, 1.0f, 1e-6f));
}

TEST_CASE("Mat4 Decompose", "[Matrix]") {
    SECTION("Simple transform") {
        Vec3 translation{1.0f, 2.0f, 3.0f};
        Vec3 scale{2.0f, 3.0f, 4.0f};

        Mat4 m = Mat4::Translation(translation) * Mat4::Scale(scale);

        Vec3 outTrans, outRot, outScale;
        m.Decompose(outTrans, outRot, outScale);

        REQUIRE(AlmostEqual(outTrans.x, translation.x, 1e-5f));
        REQUIRE(AlmostEqual(outTrans.y, translation.y, 1e-5f));
        REQUIRE(AlmostEqual(outTrans.z, translation.z, 1e-5f));

        REQUIRE(AlmostEqual(outScale.x, scale.x, 1e-5f));
        REQUIRE(AlmostEqual(outScale.y, scale.y, 1e-5f));
        REQUIRE(AlmostEqual(outScale.z, scale.z, 1e-5f));

        REQUIRE(AlmostEqual(outRot.x, 0.0f, 1e-5f));
        REQUIRE(AlmostEqual(outRot.y, 0.0f, 1e-5f));
        REQUIRE(AlmostEqual(outRot.z, 0.0f, 1e-5f));
    }

    SECTION("With rotation") {
        Vec3 translation{5.0f, 0.0f, 0.0f};
        F32 angle = PI / 6.0f;
        Vec3 scale{1.0f, 1.0f, 1.0f};

        Mat4 m = Mat4::Translation(translation) * Mat4::RotationY(angle) * Mat4::Scale(scale);

        Vec3 outTrans, outRot, outScale;
        m.Decompose(outTrans, outRot, outScale);

        REQUIRE(AlmostEqual(outTrans.x, translation.x, 1e-5f));
        REQUIRE(AlmostEqual(outTrans.y, translation.y, 1e-5f));
        REQUIRE(AlmostEqual(outTrans.z, translation.z, 1e-5f));

        REQUIRE(AlmostEqual(outScale.x, scale.x, 1e-5f));
        REQUIRE(AlmostEqual(outScale.y, scale.y, 1e-5f));
        REQUIRE(AlmostEqual(outScale.z, scale.z, 1e-5f));

        REQUIRE(AlmostEqual(outRot.y, angle, 1e-5f));
    }
}

TEST_CASE("Matrix Constants", "[Matrix]") {
    SECTION("Mat3::Identity") {
        REQUIRE(Mat3::Identity.m[0][0] == 1.0f);
        REQUIRE(Mat3::Identity.m[1][1] == 1.0f);
        REQUIRE(Mat3::Identity.m[2][2] == 1.0f);
        REQUIRE(Mat3::Identity.m[0][1] == 0.0f);
    }

    SECTION("Mat4::Identity") {
        REQUIRE(Mat4::Identity.m[0][0] == 1.0f);
        REQUIRE(Mat4::Identity.m[1][1] == 1.0f);
        REQUIRE(Mat4::Identity.m[2][2] == 1.0f);
        REQUIRE(Mat4::Identity.m[3][3] == 1.0f);
        REQUIRE(Mat4::Identity.m[0][1] == 0.0f);
    }
}