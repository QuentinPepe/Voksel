#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

import Math.Core;

using namespace Math;

TEST_CASE("Clamp borde inférieur et supérieur", "[Core]") {
    REQUIRE( Clamp( -1.0f, 0.0f, 1.0f ) ==  0.0f );
    REQUIRE( Clamp(  2.0f, 0.0f, 1.0f ) ==  1.0f );
    REQUIRE( Clamp(  0.5f, 0.0f, 1.0f ) ==  0.5f );
}

TEST_CASE("Conversion degrés ↔ radians", "[Core]") {
    constexpr float deg = 180.0f;
    constexpr float rad = ToRadians(deg);
    STATIC_REQUIRE( AlmostEqual(rad, PI) );           // test compile-time
    REQUIRE( AlmostEqual(ToDegrees(rad), deg) );
}
