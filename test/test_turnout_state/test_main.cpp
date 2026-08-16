#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/TurnoutState.h"

TEST_CASE("TurnoutState values are distinct")
{
    REQUIRE(TurnoutState::Closed != TurnoutState::Thrown);
    REQUIRE(TurnoutState::Closed != TurnoutState::Moving);
    REQUIRE(TurnoutState::Closed != TurnoutState::Unknown);
    REQUIRE(TurnoutState::Thrown != TurnoutState::Moving);
    REQUIRE(TurnoutState::Thrown != TurnoutState::Unknown);
    REQUIRE(TurnoutState::Moving != TurnoutState::Unknown);
}

TEST_CASE("TurnoutState values are equal to themselves")
{
    REQUIRE(TurnoutState::Closed == TurnoutState::Closed);
    REQUIRE(TurnoutState::Thrown == TurnoutState::Thrown);
    REQUIRE(TurnoutState::Moving == TurnoutState::Moving);
    REQUIRE(TurnoutState::Unknown == TurnoutState::Unknown);
}
