#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/TurnoutId.h"

TEST_CASE("TurnoutId reports the value it was constructed with")
{
    TurnoutId id(7);

    REQUIRE(id.value() == 7);
}

TEST_CASE("TurnoutIds with equal values are equal")
{
    REQUIRE(TurnoutId(3) == TurnoutId(3));
    REQUIRE_FALSE(TurnoutId(3) == TurnoutId(4));
}

TEST_CASE("TurnoutIds with different values are not equal")
{
    REQUIRE(TurnoutId(3) != TurnoutId(4));
    REQUIRE_FALSE(TurnoutId(3) != TurnoutId(3));
}
