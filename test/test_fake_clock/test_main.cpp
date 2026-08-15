#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "support/FakeClock.h"

TEST_CASE("FakeClock begins at zero")
{
    FakeClock clock;

    REQUIRE(clock.now() == Instant(0));
}

TEST_CASE("setNow sets the reported time")
{
    FakeClock clock;

    clock.setNow(Instant(500));

    REQUIRE(clock.now() == Instant(500));
}

TEST_CASE("advanceBy adds to the reported time")
{
    FakeClock clock;
    clock.setNow(Instant(100));

    clock.advanceBy(Duration(50));

    REQUIRE(clock.now() == Instant(150));
}

TEST_CASE("advanceBy can be called multiple times")
{
    FakeClock clock;

    clock.advanceBy(Duration(10));
    clock.advanceBy(Duration(20));
    clock.advanceBy(Duration(30));

    REQUIRE(clock.now() == Instant(60));
}
