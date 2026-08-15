#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/Level.h"

TEST_CASE("Level values are distinct")
{
    REQUIRE(Level::Low != Level::High);
}

TEST_CASE("Level values are equal to themselves")
{
    REQUIRE(Level::Low == Level::Low);
    REQUIRE(Level::High == Level::High);
}
