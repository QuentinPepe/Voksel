#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cmath>

import Math.Core;

using namespace Math;

TEST_CASE("Linear interpolation (Lerp)", "[Core]") {
    REQUIRE( Lerp( 0.0f, 1.0f, 0.0f ) == 0.0f );
    REQUIRE( Lerp( 0.0f, 1.0f, 1.0f ) == 1.0f );
    REQUIRE( Lerp( 2.0f, 4.0f, 0.5f ) == 3.0f );
}

TEST_CASE("Inverse linear interpolation (InverseLerp)", "[Core]") {
    REQUIRE( InverseLerp( 0.0f, 1.0f, 0.0f ) == 0.0f );
    REQUIRE( InverseLerp( 0.0f, 1.0f, 1.0f ) == 1.0f );
    REQUIRE( InverseLerp( 2.0f, 4.0f, 3.0f ) == 0.5f );
}

TEST_CASE("Range remapping (Remap)", "[Core]") {
    REQUIRE( Remap( 5.0f, 0.0f, 10.0f, 0.0f, 1.0f ) == 0.5f );
    REQUIRE( Remap( 0.0f,  0.0f, 10.0f, -1.0f, 1.0f ) == -1.0f );
    REQUIRE( Remap( 10.0f, 0.0f, 10.0f, -1.0f, 1.0f ) ==  1.0f );
}

TEST_CASE("Smooth step interpolation", "[Core]") {
    REQUIRE( SmoothStep( 0.0f, 1.0f, 0.0f ) == 0.0f );
    REQUIRE( SmoothStep( 0.0f, 1.0f, 1.0f ) == 1.0f );
    REQUIRE( SmoothStep( 0.0f, 1.0f, 0.5f ) == 0.5f );
}

TEST_CASE("Smoother step interpolation", "[Core]") {
    REQUIRE( SmootherStep( 0.0f, 1.0f, 0.0f ) == 0.0f );
    REQUIRE( SmootherStep( 0.0f, 1.0f, 1.0f ) == 1.0f );
    REQUIRE( SmootherStep( 0.0f, 1.0f, 0.5f ) == 0.5f );
}

TEST_CASE("Floating‐point almost‐equality", "[Core]") {
    REQUIRE( AlmostEqual( 0.0f, 1e-7f ) );
    REQUIRE( !AlmostEqual( 0.0f, 1e-4f ) );
    REQUIRE( AlmostEqual( 1.0f, 1.0f + EPSILON ) );
}

TEST_CASE("Sign function behavior", "[Core]") {
    REQUIRE( Sign( -5.0f ) == -1.0f );
    REQUIRE( Sign(  0.0f ) ==  0.0f );
    REQUIRE( Sign(  5.0f ) ==  1.0f );
}

TEST_CASE("Fast inverse square root", "[Core]") {
    REQUIRE( AlmostEqual( FastInvSqrt( 4.0f ), 0.5f, 1e-3f ) );
    REQUIRE( AlmostEqual( FastInvSqrt( 2.0f ), 1.0f / std::sqrt( 2.0f ), 1e-3f ) );
}

TEST_CASE("Wrap value into range", "[Core]") {
    REQUIRE( Wrap( -1.0f, 0.0f, 3.0f ) == 2.0f );
    REQUIRE( Wrap(  4.0f, 0.0f, 3.0f ) == 1.0f );
    REQUIRE( Wrap(  1.5f, 0.0f, 3.0f ) == 1.5f );
}

TEST_CASE("Ping-pong oscillation", "[Core]") {
    REQUIRE( PingPong( 0.5f, 1.0f ) == 0.5f );
    REQUIRE( PingPong( 1.5f, 1.0f ) == 0.5f );
    REQUIRE( PingPong( 2.5f, 1.0f ) == 0.5f );
}

