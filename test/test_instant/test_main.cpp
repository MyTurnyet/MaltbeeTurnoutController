#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/Instant.h"

TEST_CASE("Subtracting an earlier Instant yields the elapsed Duration")
{
    Instant earlier(100);
    Instant later(350);

    REQUIRE((later - earlier) == Duration(250));
}

TEST_CASE("Adding a Duration to an Instant yields a later Instant")
{
    Instant start(100);

    REQUIRE((start + Duration(50)) == Instant(150));
}

TEST_CASE("Instants with equal milliseconds are equal")
{
    REQUIRE(Instant(100) == Instant(100));
    REQUIRE_FALSE(Instant(100) == Instant(200));
    REQUIRE(Instant(100) != Instant(200));
}

TEST_CASE("Instants compare by milliseconds")
{
    REQUIRE(Instant(100) < Instant(200));
    REQUIRE(Instant(100) <= Instant(100));
    REQUIRE(Instant(200) > Instant(100));
    REQUIRE(Instant(100) >= Instant(100));
}
