#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/Orientation.h"

TEST_CASE("Normal orientation maps Closed to Low and Thrown to High")
{
    Orientation orientation = Orientation::normal();

    REQUIRE(orientation.toLevel(TurnoutPosition::closed()) == Level::Low);
    REQUIRE(orientation.toLevel(TurnoutPosition::thrown()) == Level::High);
}

TEST_CASE("Inverted orientation maps Closed to High and Thrown to Low")
{
    Orientation orientation = Orientation::inverted();

    REQUIRE(orientation.toLevel(TurnoutPosition::closed()) == Level::High);
    REQUIRE(orientation.toLevel(TurnoutPosition::thrown()) == Level::Low);
}

TEST_CASE("Normal orientation maps Low back to Closed and High back to Thrown")
{
    Orientation orientation = Orientation::normal();

    REQUIRE(orientation.toPosition(Level::Low) == TurnoutPosition::closed());
    REQUIRE(orientation.toPosition(Level::High) == TurnoutPosition::thrown());
}

TEST_CASE("Inverted orientation maps Low back to Thrown and High back to Closed")
{
    Orientation orientation = Orientation::inverted();

    REQUIRE(orientation.toPosition(Level::Low) == TurnoutPosition::thrown());
    REQUIRE(orientation.toPosition(Level::High) == TurnoutPosition::closed());
}

TEST_CASE("Round-trip through normal orientation returns the original position")
{
    Orientation orientation = Orientation::normal();

    REQUIRE(orientation.toPosition(orientation.toLevel(TurnoutPosition::closed())) == TurnoutPosition::closed());
    REQUIRE(orientation.toPosition(orientation.toLevel(TurnoutPosition::thrown())) == TurnoutPosition::thrown());
}

TEST_CASE("Round-trip through inverted orientation returns the original position")
{
    Orientation orientation = Orientation::inverted();

    REQUIRE(orientation.toPosition(orientation.toLevel(TurnoutPosition::closed())) == TurnoutPosition::closed());
    REQUIRE(orientation.toPosition(orientation.toLevel(TurnoutPosition::thrown())) == TurnoutPosition::thrown());
}
