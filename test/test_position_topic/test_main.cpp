#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/PositionTopic.h"

TEST_CASE("the position topic is mcs/turnout/<address>/position")
{
    REQUIRE(PositionTopic(41).value() == "mcs/turnout/41/position");
}

TEST_CASE("the position topic uses the address's decimal form")
{
    REQUIRE(PositionTopic(2048).value() == "mcs/turnout/2048/position");
}
