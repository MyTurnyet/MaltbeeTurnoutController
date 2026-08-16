#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/TurnoutPosition.h"

TEST_CASE("closed() and thrown() are distinct")
{
    REQUIRE(TurnoutPosition::closed() != TurnoutPosition::thrown());
}

TEST_CASE("A TurnoutPosition equals itself")
{
    REQUIRE(TurnoutPosition::closed() == TurnoutPosition::closed());
    REQUIRE(TurnoutPosition::thrown() == TurnoutPosition::thrown());
}

TEST_CASE("opposite() of closed is thrown")
{
    REQUIRE(TurnoutPosition::closed().opposite() == TurnoutPosition::thrown());
}

TEST_CASE("opposite() of thrown is closed")
{
    REQUIRE(TurnoutPosition::thrown().opposite() == TurnoutPosition::closed());
}

TEST_CASE("opposite() is its own inverse")
{
    REQUIRE(TurnoutPosition::closed().opposite().opposite() == TurnoutPosition::closed());
}
