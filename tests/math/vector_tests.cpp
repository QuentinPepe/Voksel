#include <catch2/catch.hpp>
#include <cmath>

import Math.Core;
import Math.Vector;

using namespace Math;

TEST_CASE("Vec2 basic operations", "[Vector]") {
    Vec2 a{1.0f, 2.0f}, b{3.0f, 4.0f};
    REQUIRE((a + b) == Vec2{4.0f, 6.0f});
    REQUIRE((a - b) == Vec2{-2.0f, -2.0f});
    REQUIRE((a * 2.0f) == Vec2{2.0f, 4.0f});
    REQUIRE((2.0f * a) == Vec2{2.0f, 4.0f});
    REQUIRE((a / 2.0f) == Vec2{0.5f, 1.0f});
    REQUIRE((-a) == Vec2{-1.0f, -2.0f});
    Vec2 c{a}; c += b; REQUIRE(c == Vec2{4.0f, 6.0f});
    c = a; c -= b; REQUIRE(c == Vec2{-2.0f, -2.0f});
    c = a; c *= 2.0f; REQUIRE(c == Vec2{2.0f, 4.0f});
    c = a; c /= 2.0f; REQUIRE(c == Vec2{0.5f, 1.0f});
    REQUIRE(a == Vec2{1.0f, 2.0f});
    REQUIRE(a != b);
    REQUIRE(a[0] == 1.0f);
    REQUIRE(a[1] == 2.0f);
    REQUIRE(a.Dot(b) == 11.0f);
    REQUIRE(a.LengthSquared() == Approx(5.0f));
    REQUIRE(a.Length() == Approx(std::sqrt(5.0f)));
    Vec2 norm = Vec2{3.0f, 4.0f}.Normalized(); REQUIRE(norm == Vec2{0.6f, 0.8f});
    Vec2 perp = Vec2{1.0f, 2.0f}.Perpendicular(); REQUIRE(perp == Vec2{-2.0f, 1.0f});
    REQUIRE(Vec2::Lerp(a, b, 0.5f) == Vec2{2.0f, 3.0f});
}

TEST_CASE("Vec3 basic operations", "[Vector]") {
    Vec3 a{1.0f, 2.0f, 3.0f}, b{2.0f, 3.0f, 4.0f};
    REQUIRE((a + b) == Vec3{3.0f, 5.0f, 7.0f});
    REQUIRE((a - b) == Vec3{-1.0f, -1.0f, -1.0f});
    REQUIRE((a * b) == Vec3{2.0f, 6.0f, 12.0f});
    REQUIRE((a * 2.0f) == Vec3{2.0f, 4.0f, 6.0f});
    REQUIRE((a / 2.0f) == Vec3{0.5f, 1.0f, 1.5f});
    REQUIRE((-a) == Vec3{-1.0f, -2.0f, -3.0f});
    Vec3 c{a}; c += b; REQUIRE(c == Vec3{3.0f, 5.0f, 7.0f});
    c = a; c -= b; REQUIRE(c == Vec3{-1.0f, -1.0f, -1.0f});
    c = a; c *= 2.0f; REQUIRE(c == Vec3{2.0f, 4.0f, 6.0f});
    c = a; c /= 2.0f; REQUIRE(c == Vec3{0.5f, 1.0f, 1.5f});
    REQUIRE(a == Vec3{1.0f, 2.0f, 3.0f});
    REQUIRE(a != b);
    REQUIRE(a[0] == 1.0f);
    REQUIRE(a[1] == 2.0f);
    REQUIRE(a[2] == 3.0f);
    REQUIRE(a.Dot(b) == Approx(20.0f));
    REQUIRE(a.Cross(b) == Vec3{-1.0f, 2.0f, -1.0f});
    REQUIRE(a.LengthSquared() == Approx(14.0f));
    REQUIRE(a.Length() == Approx(std::sqrt(14.0f)));
    Vec3 norm = Vec3{0.0f, 3.0f, 4.0f}.Normalized(); REQUIRE(norm == Vec3{0.0f, 0.6f, 0.8f});
    REQUIRE(Vec3::Lerp(a, b, 0.5f) == Vec3{1.5f, 2.5f, 3.5f});
    Vec3 slerp = Vec3::Slerp(Vec3::UnitX, Vec3::UnitY, 0.5f);
    REQUIRE(Approx(slerp.Length()) == 1.0f);
    REQUIRE(a.XY() == Vec2{1.0f, 2.0f});
    REQUIRE(a.XZ() == Vec2{1.0f, 3.0f});
    REQUIRE(a.YZ() == Vec2{2.0f, 3.0f});
}

TEST_CASE("Vec4 basic operations", "[Vector]") {
    Vec4 a{1.0f, 2.0f, 3.0f, 4.0f}, b{2.0f, 3.0f, 4.0f, 5.0f};
    REQUIRE((a + b) == Vec4{3.0f, 5.0f, 7.0f, 9.0f});
    REQUIRE((a - b) == Vec4{-1.0f, -1.0f, -1.0f, -1.0f});
    REQUIRE((a * 2.0f) == Vec4{2.0f, 4.0f, 6.0f, 8.0f});
    REQUIRE((a / 2.0f) == Vec4{0.5f, 1.0f, 1.5f, 2.0f});
    REQUIRE((-a) == Vec4{-1.0f, -2.0f, -3.0f, -4.0f});
    REQUIRE(a.Dot(b) == Approx(40.0f));
    REQUIRE(a.LengthSquared() == Approx(30.0f));
    REQUIRE(a.Length() == Approx(std::sqrt(30.0f)));
    Vec4 norm = Vec4{0.0f, 0.0f, 0.0f, 2.0f}.Normalized(); REQUIRE(norm == Vec4{0.0f, 0.0f, 0.0f, 1.0f});
    REQUIRE(Vec4::Lerp(a, b, 0.5f) == Vec4{1.5f, 2.5f, 3.5f, 4.5f});
    REQUIRE(a[3] == 4.0f);
}

TEST_CASE("Integer vector conversions", "[Vector]") {
    IVec2 i2{2, 3}; REQUIRE((i2 + IVec2{1, 1}) == IVec2{3, 4});
    REQUIRE((i2 - IVec2{1, 2}) == IVec2{1, 1});
    REQUIRE((i2 * 2) == IVec2{4, 6});
    REQUIRE((i2 / 2) == IVec2{1, 1});
    REQUIRE((-i2) == IVec2{-2, -3});
    REQUIRE(i2 == IVec2{2, 3}); REQUIRE(i2 != IVec2{3, 2});
    REQUIRE(i2.ToFloat() == Vec2{2.0f, 3.0f});

    IVec3 i3{2, 3, 4}; REQUIRE((i3 + IVec3{1, 1, 1}) == IVec3{3, 4, 5});
    REQUIRE((i3 - IVec3{1, 2, 3}) == IVec3{1, 1, 1});
    REQUIRE((i3 * 2) == IVec3{4, 6, 8});
    REQUIRE((i3 / 2) == IVec3{1, 1, 2});
    REQUIRE((-i3) == IVec3{-2, -3, -4});
    REQUIRE(i3 == IVec3{2, 3, 4}); REQUIRE(i3 != IVec3{4, 3, 2});
    REQUIRE(i3.ToFloat() == Vec3{2.0f, 3.0f, 4.0f});
}
