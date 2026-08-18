#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/SteadyBlinker.h"
#include "domain/Duration.h"
#include "domain/Level.h"

TEST_CASE("SteadyBlinker is High for the first half of each period")
{
    SteadyBlinker blinker(Duration(250));

    REQUIRE(blinker.levelAt(Duration(0)) == Level::High);
    REQUIRE(blinker.levelAt(Duration(100)) == Level::High);
    REQUIRE(blinker.levelAt(Duration(249)) == Level::High);
}

TEST_CASE("SteadyBlinker is Low for the second half of each period")
{
    SteadyBlinker blinker(Duration(250));

    REQUIRE(blinker.levelAt(Duration(250)) == Level::Low);
    REQUIRE(blinker.levelAt(Duration(400)) == Level::Low);
    REQUIRE(blinker.levelAt(Duration(499)) == Level::Low);
}

TEST_CASE("SteadyBlinker repeats indefinitely")
{
    SteadyBlinker blinker(Duration(250));

    REQUIRE(blinker.levelAt(Duration(500)) == Level::High);
    REQUIRE(blinker.levelAt(Duration(750)) == Level::Low);
    REQUIRE(blinker.levelAt(Duration(1000)) == Level::High);
}
