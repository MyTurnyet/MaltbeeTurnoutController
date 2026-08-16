#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/TurnoutConfig.h"
#include "domain/TurnoutId.h"
#include "domain/Orientation.h"
#include "domain/Duration.h"

TEST_CASE("TurnoutConfig reports the fields it was constructed with")
{
    TurnoutConfig config(TurnoutId(1), 13, 36, Orientation::normal(), Duration(50), Duration(200));

    REQUIRE(config.id() == TurnoutId(1));
    REQUIRE(config.outputPin() == 13);
    REQUIRE(config.feedbackPin() == 36);
    REQUIRE(config.settleDuration() == Duration(50));
    REQUIRE(config.movementTimeout() == Duration(200));
}

TEST_CASE("TurnoutConfigs with equal fields are equal")
{
    TurnoutConfig a(TurnoutId(1), 13, 36, Orientation::normal(), Duration(50), Duration(200));
    TurnoutConfig b(TurnoutId(1), 13, 36, Orientation::normal(), Duration(50), Duration(200));

    REQUIRE(a == b);
}

TEST_CASE("TurnoutConfigs differing only by id are not equal")
{
    TurnoutConfig a(TurnoutId(1), 13, 36, Orientation::normal(), Duration(50), Duration(200));
    TurnoutConfig b(TurnoutId(2), 13, 36, Orientation::normal(), Duration(50), Duration(200));

    REQUIRE(a != b);
}

TEST_CASE("TurnoutConfigs differing only by output pin are not equal")
{
    TurnoutConfig a(TurnoutId(1), 13, 36, Orientation::normal(), Duration(50), Duration(200));
    TurnoutConfig b(TurnoutId(1), 14, 36, Orientation::normal(), Duration(50), Duration(200));

    REQUIRE(a != b);
}

TEST_CASE("TurnoutConfigs differing only by orientation are not equal")
{
    TurnoutConfig a(TurnoutId(1), 13, 36, Orientation::normal(), Duration(50), Duration(200));
    TurnoutConfig b(TurnoutId(1), 13, 36, Orientation::inverted(), Duration(50), Duration(200));

    REQUIRE(a != b);
}
