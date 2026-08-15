#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/Duration.h"

TEST_CASE("Duration reports the milliseconds it was constructed with")
{
    Duration duration(150);

    REQUIRE(duration.milliseconds() == 150);
}

TEST_CASE("Durations with equal milliseconds are equal")
{
    REQUIRE(Duration(100) == Duration(100));
    REQUIRE_FALSE(Duration(100) == Duration(200));
    REQUIRE(Duration(100) != Duration(200));
}

TEST_CASE("Durations compare by milliseconds")
{
    REQUIRE(Duration(100) < Duration(200));
    REQUIRE(Duration(100) <= Duration(100));
    REQUIRE(Duration(200) > Duration(100));
    REQUIRE(Duration(100) >= Duration(100));
}
