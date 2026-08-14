#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "support/FakeClock.h"

TEST_CASE("FakeClock begins at zero")
{
    FakeClock clock;

    REQUIRE(clock.nowMillis() == 0);
}

TEST_CASE("setNow sets the reported time")
{
    FakeClock clock;

    clock.setNow(500);

    REQUIRE(clock.nowMillis() == 500);
}

TEST_CASE("advanceBy adds to the reported time")
{
    FakeClock clock;
    clock.setNow(100);

    clock.advanceBy(50);

    REQUIRE(clock.nowMillis() == 150);
}

TEST_CASE("advanceBy can be called multiple times")
{
    FakeClock clock;

    clock.advanceBy(10);
    clock.advanceBy(20);
    clock.advanceBy(30);

    REQUIRE(clock.nowMillis() == 60);
}
